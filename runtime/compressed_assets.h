#ifndef NARAKU_COMPRESSED_ASSETS_H
#define NARAKU_COMPRESSED_ASSETS_H

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef NARAKU_COMPRESSED_ASSETS
#include <zlib.h>
#endif

typedef struct {
    FILE *file;
#ifdef NARAKU_COMPRESSED_ASSETS
    z_stream stream;
    uint8_t input[2048];
    int compressed;
    int ended;
#endif
} PcmReader;

static int pcm_reader_open(PcmReader *reader, const char *path)
{
    memset(reader, 0, sizeof(*reader));
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

static size_t pcm_reader_read(PcmReader *reader, void *dst, size_t bytes)
{
    if (!reader->file || !dst || !bytes) return 0;
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

static int pcm_reader_rewind(PcmReader *reader)
{
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
#ifdef NARAKU_COMPRESSED_ASSETS
    if (reader->compressed) inflateEnd(&reader->stream);
#endif
    if (reader->file) fclose(reader->file);
    memset(reader, 0, sizeof(*reader));
}

#endif
