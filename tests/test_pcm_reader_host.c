#include "../runtime/compressed_assets.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    PcmReader reader;
    FILE *expected;
    uint8_t actual[3072];
    uint8_t reference[3072];
    size_t got, want;
    if (argc != 2) return 2;
    expected = fopen(argv[1], "rb");
    if (!expected || !pcm_reader_open(&reader, argv[1])) return 3;
    for (;;) {
        got = pcm_reader_read(&reader, actual, sizeof(actual));
        want = fread(reference, 1, sizeof(reference), expected);
        if (got != want || (got && memcmp(actual, reference, got) != 0)) return 4;
        if (!got) break;
    }
    if (!pcm_reader_rewind(&reader)) return 5;
    rewind(expected);
    got = pcm_reader_read(&reader, actual, sizeof(actual));
    want = fread(reference, 1, sizeof(reference), expected);
    if (got != want || (got && memcmp(actual, reference, got) != 0)) return 6;
    pcm_reader_close(&reader);
    fclose(expected);
    return 0;
}
