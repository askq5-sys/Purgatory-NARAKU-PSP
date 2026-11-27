/* NAUD audio: lossless compression for music and sound effects.
 *
 * Self-contained: used by the runtime (runtime/compressed_assets.h), by
 * tools/pack_audio.py (through a small C driver) and by the tests, so the
 * encoder and the decoder the PSP runs are the same code.
 *
 * Each block of samples is predicted from the previous ones (fixed
 * polynomial predictors of order 0-3, like FLAC) and the prediction errors
 * are Rice coded. Quiet, slow music compresses 2-17x; nothing is lost.
 *
 * File layout (little endian):
 *   0  "NAUD"
 *   4  u8  version (1)
 *   5  u8  channels (1 or 2; mono is played on both sides)
 *   6  u16 frames per block
 *   8  u32 total frames
 *  12  u16 gain, 4096 = 1.0 (applied while decoding)
 *  14  u8  flags: bit 0 = alias
 *  15  u8  reserved
 *  16  blocks, or for an alias the file name (in the same folder) of the
 *      file holding the samples, NUL-terminated: a quieter copy of a sound
 *      is stored as that file plus a gain.
 *
 * Block: u16 payload bytes, then the payload:
 *   u8 stereo mode (0 = left/right, 1 = mid/side), then per channel
 *   u8 order (0-3, or 255 = verbatim s16 samples), u8 Rice parameter,
 *   order warm-up samples (s32), and the Rice-coded residuals of the other
 *   samples, MSB first, padded to a byte. A residual r is stored as
 *   u = 2r (r >= 0) or -2r-1: u >> k zero bits, a one bit, then the low k
 *   bits of u. The last block holds the frames left (total frames says). */
#ifndef NARAKU_AUDIO_H
#define NARAKU_AUDIO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define NAUD_HEADER_BYTES 16
#define NAUD_FLAG_ALIAS 1
#define NAUD_GAIN_ONE 4096
#define NAUD_MAX_CHANNELS 2
#define NAUD_ALIAS_NAME_MAX 48
#define NAUD_VERBATIM 255

typedef struct {
    int channels;
    int block_frames;
    uint32_t total_frames;
    int gain;
    int alias;
} NaudHeader;

static inline int naud_clamp16(int32_t v)
{
    return v < -32768 ? -32768 : v > 32767 ? 32767 : (int)v;
}

/* Largest payload a block may have: verbatim samples plus the small
 * per-channel headers. The encoder falls back to verbatim above it. */
static inline size_t naud_max_payload(int channels, int block_frames)
{
    return 1u + (size_t)channels * (2u + (size_t)block_frames * 2u);
}

static inline int naud_parse_header(const uint8_t *h, NaudHeader *out)
{
    if (h[0] != 'N' || h[1] != 'A' || h[2] != 'U' || h[3] != 'D' || h[4] != 1)
        return 0;
    out->channels = h[5];
    out->block_frames = h[6] | (h[7] << 8);
    out->total_frames = (uint32_t)h[8] | ((uint32_t)h[9] << 8) |
                        ((uint32_t)h[10] << 16) | ((uint32_t)h[11] << 24);
    out->gain = h[12] | (h[13] << 8);
    out->alias = (h[14] & NAUD_FLAG_ALIAS) != 0;
    if (out->channels < 1 || out->channels > NAUD_MAX_CHANNELS) return 0;
    if (out->block_frames < 4 ||
        naud_max_payload(out->channels, out->block_frames) > 65535u) return 0;
    return 1;
}

static inline int32_t naud_predict(const int32_t *s, int i, int order)
{
    switch (order) {
    case 1: return s[i - 1];
    case 2: return 2 * s[i - 1] - s[i - 2];
    case 3: return 3 * s[i - 1] - 3 * s[i - 2] + s[i - 3];
    default: return 0;
    }
}

/* Decodes one channel; returns bytes used, or 0 if the data is bad. */
static inline size_t naud_decode_channel(const uint8_t *p, size_t len, int frames, int32_t *s)
{
    size_t pos = 2;
    int order, k, i;
    uint32_t bits = 0;
    int have = 0;
    if (len < 2) return 0;
    order = p[0];
    k = p[1];
    if (order == NAUD_VERBATIM) {
        if (len < 2u + (size_t)frames * 2u) return 0;
        for (i = 0; i < frames; ++i)
            s[i] = (int16_t)(p[2 + 2 * i] | (p[3 + 2 * i] << 8));
        return 2u + (size_t)frames * 2u;
    }
    if (order > 3 || k > 24) return 0;
    if (order > frames) order = frames;
    if (len < pos + (size_t)order * 4u) return 0;
    for (i = 0; i < order; ++i, pos += 4)
        s[i] = (int32_t)((uint32_t)p[pos] | ((uint32_t)p[pos + 1] << 8) |
                         ((uint32_t)p[pos + 2] << 16) | ((uint32_t)p[pos + 3] << 24));
    for (i = order; i < frames; ++i) {
        uint32_t q = 0, u;
        /* unary part: count zero bits up to the terminating one */
        for (;;) {
            if (!have) {
                if (pos >= len) return 0;
                bits = p[pos++];
                have = 8;
            }
            if (bits & (1u << (have - 1))) { --have; break; }
            --have;
            if (++q > (1u << 20)) return 0;
        }
        u = q << k;
        if (k) {
            int need = k;
            uint32_t low = 0;
            while (need) {
                int take;
                if (!have) {
                    if (pos >= len) return 0;
                    bits = p[pos++];
                    have = 8;
                }
                take = need < have ? need : have;
                low = (low << take) | ((bits >> (have - take)) & ((1u << take) - 1u));
                have -= take;
                need -= take;
            }
            u |= low;
        }
        s[i] = naud_predict(s, i, order) + (int32_t)((u & 1u) ? ~(u >> 1) : (u >> 1));
    }
    return pos;
}

/* Decodes one block payload into interleaved stereo; returns frames or 0. */
static inline int naud_decode_block(const uint8_t *p, size_t len, const NaudHeader *h,
                                    int frames, int gain, int32_t *scratch, int16_t *out)
{
    int32_t *ch[2];
    size_t pos = 1;
    int c, i, mode;
    if (!len || frames <= 0 || frames > h->block_frames) return 0;
    mode = p[0];
    ch[0] = scratch;
    ch[1] = scratch + h->block_frames;
    for (c = 0; c < h->channels; ++c) {
        size_t used = naud_decode_channel(p + pos, len - pos, frames, ch[c]);
        if (!used) return 0;
        pos += used;
    }
    for (i = 0; i < frames; ++i) {
        int32_t l, r;
        if (h->channels == 1) {
            l = r = ch[0][i];
        } else if (mode == 1) {
            int32_t side = ch[1][i];
            int32_t mid = (int32_t)((uint32_t)ch[0][i] << 1) | (side & 1);
            l = (mid + side) >> 1;
            r = (mid - side) >> 1;
        } else {
            l = ch[0][i];
            r = ch[1][i];
        }
        if (gain != NAUD_GAIN_ONE) {
            l = (l * gain + 2048) >> 12;
            r = (r * gain + 2048) >> 12;
        }
        out[2 * i] = (int16_t)naud_clamp16(l);
        out[2 * i + 1] = (int16_t)naud_clamp16(r);
    }
    return frames;
}

/* ---- Encoder (tools and tests; not called by the EBOOT) ---- */

typedef struct {
    uint8_t *p;
    size_t pos;
    uint32_t acc;
    int have;
} NaudBits;

static inline void naud_put_bits(NaudBits *b, uint32_t v, int n)
{
    while (n--) {
        b->acc = (b->acc << 1) | ((v >> n) & 1u);
        if (++b->have == 8) {
            b->p[b->pos++] = (uint8_t)b->acc;
            b->acc = 0;
            b->have = 0;
        }
    }
}

static inline uint64_t naud_rice_cost(const int32_t *s, int frames, int order, int k)
{
    uint64_t bits = 0;
    int i;
    for (i = order; i < frames; ++i) {
        int32_t r = s[i] - naud_predict(s, i, order);
        uint32_t u = r >= 0 ? (uint32_t)r << 1 : ((uint32_t)(-(r + 1)) << 1) | 1u;
        bits += (u >> k) + 1u + (uint32_t)k;
    }
    return bits + (uint64_t)order * 32u;
}

/* Encodes one channel at dst; returns bytes written (never more than the
 * verbatim size). */
static inline size_t naud_encode_channel(const int32_t *s, int frames, int may_verbatim, uint8_t *dst)
{
    int order, k, best_order = 0, best_k = 0, i;
    uint64_t best = ~(uint64_t)0;
    NaudBits b;
    for (order = 0; order <= 3 && order <= frames; ++order) {
        /* The best Rice parameter is near log2 of the mean residual. */
        uint64_t sum = 0;
        int k0 = 0;
        for (i = order; i < frames; ++i) {
            int32_t r = s[i] - naud_predict(s, i, order);
            sum += r >= 0 ? (uint64_t)r << 1 : ((uint64_t)(-(int64_t)r) << 1) - 1u;
        }
        if (frames > order) {
            uint64_t mean = sum / (uint64_t)(frames - order);
            while (k0 < 20 && (mean >> (k0 + 1))) ++k0;
        }
        for (k = k0 > 0 ? k0 - 1 : 0; k <= k0 + 1 && k <= 20; ++k) {
            uint64_t c = naud_rice_cost(s, frames, order, k);
            if (c < best) { best = c; best_order = order; best_k = k; }
        }
    }
    if (may_verbatim && (best + 7) / 8 + 2 >= 2u + (size_t)frames * 2u) {
        dst[0] = NAUD_VERBATIM;
        dst[1] = 0;
        for (i = 0; i < frames; ++i) {
            dst[2 + 2 * i] = (uint8_t)(s[i] & 255);
            dst[3 + 2 * i] = (uint8_t)((s[i] >> 8) & 255);
        }
        return 2u + (size_t)frames * 2u;
    }
    dst[0] = (uint8_t)best_order;
    dst[1] = (uint8_t)best_k;
    b.p = dst;
    b.pos = 2;
    b.acc = 0;
    b.have = 0;
    for (i = 0; i < best_order; ++i) {
        uint32_t v = (uint32_t)s[i];
        b.p[b.pos++] = (uint8_t)v;
        b.p[b.pos++] = (uint8_t)(v >> 8);
        b.p[b.pos++] = (uint8_t)(v >> 16);
        b.p[b.pos++] = (uint8_t)(v >> 24);
    }
    for (i = best_order; i < frames; ++i) {
        int32_t r = s[i] - naud_predict(s, i, best_order);
        uint32_t u = r >= 0 ? (uint32_t)r << 1 : ((uint32_t)(-(r + 1)) << 1) | 1u;
        uint32_t q = u >> best_k;
        while (q >= 32) { naud_put_bits(&b, 0, 32); q -= 32; }
        naud_put_bits(&b, 1, (int)q + 1);
        if (best_k) naud_put_bits(&b, u & ((1u << best_k) - 1u), best_k);
    }
    if (b.have) naud_put_bits(&b, 0, 8 - b.have);
    return b.pos;
}

/* Encodes interleaved s16 frames (`channels` per frame) into a payload;
 * dst must hold naud_max_payload() bytes plus slack for the trial encode.
 * Returns the payload size. */
static inline size_t naud_encode_block(const int16_t *in, int channels, int frames,
                                       int32_t *scratch, uint8_t *dst, uint8_t *trial)
{
    int32_t *a = scratch, *b = scratch + frames, *m = scratch + 2 * frames, *sd = scratch + 3 * frames;
    size_t lr, ms;
    int i;
    for (i = 0; i < frames; ++i) {
        a[i] = in[(size_t)i * channels];
        b[i] = channels == 2 ? in[(size_t)i * 2 + 1] : 0;
    }
    if (channels == 1) {
        dst[0] = 0;
        return 1 + naud_encode_channel(a, frames, 1, dst + 1);
    }
    for (i = 0; i < frames; ++i) {
        m[i] = (a[i] + b[i]) >> 1;
        sd[i] = a[i] - b[i];
    }
    dst[0] = 0;
    lr = 1 + naud_encode_channel(a, frames, 1, dst + 1);
    lr += naud_encode_channel(b, frames, 1, dst + lr);
    trial[0] = 1;
    /* side needs 17 bits, so mid/side is only used when it is Rice coded */
    ms = 1 + naud_encode_channel(m, frames, 1, trial + 1);
    if (trial[1] != NAUD_VERBATIM) {
        size_t side = naud_encode_channel(sd, frames, 0, trial + ms);
        if (trial[ms] != NAUD_VERBATIM && ms + side < lr) {
            memcpy(dst, trial, ms + side);
            return ms + side;
        }
    }
    return lr;
}

#endif
