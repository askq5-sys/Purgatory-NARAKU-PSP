#ifndef NARAKU_COMPRESSED_ASSETS_H
#define NARAKU_COMPRESSED_ASSETS_H

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "naraku_audio.h"

#ifdef NARAKU_COMPRESSED_ASSETS
#include <zlib.h>
#endif

/* Audio is read through PcmReader as 16-bit stereo. For NAME.pcm it first
 * looks for NAME.nau (lossless, tools/pack_audio.py), then NAME.pcm.z in
 * compressed builds, then NAME.pcm itself. */
typedef struct {
    FILE *file;
    int naud;
    NaudHeader naud_header;
    int naud_gain;
    uint32_t naud_done;
    uint8_t *naud_payload;
    int32_t *naud_scratch;
    int16_t *naud_pcm;
    int naud_pos, naud_len;
#ifdef NARAKU_COMPRESSED_ASSETS
    z_stream stream;
    uint8_t input[2048];
    int compressed;
    int ended;
#endif
} PcmReader;

static FILE *naud_open_header(const char *path, NaudHeader *header, char *alias, size_t alias_size)
{
    uint8_t raw[NAUD_HEADER_BYTES];
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fread(raw, 1, sizeof(raw), f) != sizeof(raw) || !naud_parse_header(raw, header)) {
        fclose(f);
        return NULL;
    }
    if (header->alias) {
        size_t n = fread(alias, 1, alias_size - 1, f);
        alias[n] = '\0';
        if (!n) {
            fclose(f);
            return NULL;
        }
    }
    return f;
}

static int pcm_reader_open_naud(PcmReader *reader, const char *path)
{
    char naud_path[256], alias[NAUD_ALIAS_NAME_MAX];
    size_t len = strlen(path);
    NaudHeader header;
    int gain;
    FILE *f;
    if (len < 4 || len >= sizeof(naud_path) || strcmp(path + len - 4, ".pcm") != 0) return 0;
    memcpy(naud_path, path, len - 4);
    memcpy(naud_path + len - 4, ".nau", 5);
    f = naud_open_header(naud_path, &header, alias, sizeof(alias));
    if (!f) return 0;
    gain = header.gain;
    if (header.alias) {
        /* The samples live in another file of the same folder. */
        const char *slash = strrchr(naud_path, '/');
        size_t dir = slash ? (size_t)(slash - naud_path) + 1 : 0;
        fclose(f);
        if (dir + strlen(alias) >= sizeof(naud_path)) return 0;
        memcpy(naud_path + dir, alias, strlen(alias) + 1);
        f = naud_open_header(naud_path, &header, alias, sizeof(alias));
        if (!f) return 0;
        if (header.alias) { fclose(f); return 0; }
        gain = (gain * header.gain + 2048) >> 12;
    }
    reader->naud_payload = (uint8_t *)malloc(naud_max_payload(header.channels, header.block_frames));
    reader->naud_scratch = (int32_t *)malloc(sizeof(int32_t) * 2u * (size_t)header.block_frames);
    reader->naud_pcm = (int16_t *)malloc((size_t)header.block_frames * 4u);
    if (!reader->naud_payload || !reader->naud_scratch || !reader->naud_pcm) {
        free(reader->naud_payload);
        free(reader->naud_scratch);
        free(reader->naud_pcm);
        reader->naud_payload = NULL;
        reader->naud_scratch = NULL;
        reader->naud_pcm = NULL;
        fclose(f);
        return 0;
    }
    reader->file = f;
    reader->naud = 1;
    reader->naud_header = header;
    reader->naud_gain = gain;
    return 1;
}

static int pcm_reader_open(PcmReader *reader, const char *path)
{
    memset(reader, 0, sizeof(*reader));
    if (pcm_reader_open_naud(reader, path)) return 1;
#ifdef NARAKU_COMPRESSED_ASSETS
    {
        char compressed_path[256];
        int n = snprintf(compressed_path, sizeof(compressed_path), "%s.z", path);
        if (n > 0 && (size_t)n < sizeof(compressed_path))
            reader->file = fopen(compressed_path, "rb");
    }
    if (reader->file) {
        memset(&reader->stream, 0, sizeof(reader->stream));
        if (inflateInit(&reader->stream) != Z_OK) {
            fclose(reader->file);
            reader->file = NULL;
            return 0;
        }
        reader->compressed = 1;
        return 1;
    }
#endif
    reader->file = fopen(path, "rb");
    return reader->file != NULL;
}

static size_t naud_reader_read(PcmReader *reader, uint8_t *dst, size_t bytes)
{
    const NaudHeader *h = &reader->naud_header;
    size_t done = 0;
    bytes &= ~(size_t)3;
    while (done < bytes) {
        size_t take;
        if (reader->naud_pos >= reader->naud_len) {
            uint32_t left = h->total_frames - reader->naud_done;
            uint8_t size[2];
            size_t len;
            int frames;
            if (!left) break;
            if (fread(size, 1, 2, reader->file) != 2) break;
            len = (size_t)(size[0] | (size[1] << 8));
            if (len > naud_max_payload(h->channels, h->block_frames) ||
                fread(reader->naud_payload, 1, len, reader->file) != len) break;
            frames = left < (uint32_t)h->block_frames ? (int)left : h->block_frames;
            if (naud_decode_block(reader->naud_payload, len, h, frames, reader->naud_gain,
                                  reader->naud_scratch, reader->naud_pcm) != frames) break;
            reader->naud_len = frames * 4;
            reader->naud_pos = 0;
            reader->naud_done += (uint32_t)frames;
        }
        take = (size_t)(reader->naud_len - reader->naud_pos);
        if (take > bytes - done) take = bytes - done;
        memcpy(dst + done, (uint8_t *)reader->naud_pcm + reader->naud_pos, take);
        reader->naud_pos += (int)take;
        done += take;
    }
    return done;
}

static size_t pcm_reader_read(PcmReader *reader, void *dst, size_t bytes)
{
    if (!reader->file || !dst || !bytes) return 0;
    if (reader->naud) return naud_reader_read(reader, (uint8_t *)dst, bytes);
#ifdef NARAKU_COMPRESSED_ASSETS
    if (reader->compressed) {
        uInt capacity = (uInt)(bytes > UINT_MAX ? UINT_MAX : bytes);
        reader->stream.next_out = (Bytef *)dst;
        reader->stream.avail_out = capacity;
        while (reader->stream.avail_out && !reader->ended) {
            uInt avail_in_before;
            uInt avail_out_before;
            int result;
            if (!reader->stream.avail_in) {
                size_t got = fread(reader->input, 1, sizeof(reader->input), reader->file);
                if (!got) break;
                reader->stream.next_in = reader->input;
                reader->stream.avail_in = (uInt)got;
            }
            avail_in_before = reader->stream.avail_in;
            avail_out_before = reader->stream.avail_out;
            result = inflate(&reader->stream, Z_NO_FLUSH);
            if (result == Z_STREAM_END) reader->ended = 1;
            else if (result != Z_OK) break;
            if (result == Z_OK && avail_in_before == reader->stream.avail_in &&
                avail_out_before == reader->stream.avail_out) break;
        }
        return (size_t)(capacity - reader->stream.avail_out);
    }
#endif
    return fread(dst, 1, bytes, reader->file);
}

/* True once the whole sound has been read (the music then loops). */
static int pcm_reader_finished(PcmReader *reader)
{
    if (!reader->file) return 1;
    if (reader->naud)
        return reader->naud_done >= reader->naud_header.total_frames &&
               reader->naud_pos >= reader->naud_len;
#ifdef NARAKU_COMPRESSED_ASSETS
    if (reader->compressed) return reader->ended || feof(reader->file);
#endif
    return feof(reader->file);
}

static int pcm_reader_rewind(PcmReader *reader)
{
    if (reader->naud) {
        if (!reader->file || fseek(reader->file, NAUD_HEADER_BYTES, SEEK_SET) != 0) return 0;
        reader->naud_done = 0;
        reader->naud_pos = reader->naud_len = 0;
        return 1;
    }
    if (!reader->file || fseek(reader->file, 0, SEEK_SET) != 0) return 0;
#ifdef NARAKU_COMPRESSED_ASSETS
    if (reader->compressed) {
        inflateEnd(&reader->stream);
        memset(&reader->stream, 0, sizeof(reader->stream));
        reader->ended = 0;
        if (inflateInit(&reader->stream) != Z_OK) return 0;
    }
#endif
    return 1;
}

static void pcm_reader_close(PcmReader *reader)
{
    if (!reader) return;
    free(reader->naud_payload);
    free(reader->naud_scratch);
    free(reader->naud_pcm);
#ifdef NARAKU_COMPRESSED_ASSETS
    if (reader->compressed) inflateEnd(&reader->stream);
#endif
    if (reader->file) fclose(reader->file);
    memset(reader, 0, sizeof(*reader));
}

#endif
