#include "base64_decoder.h"
#include <string.h>
#include <stdlib.h>

// Base64 decoding table
static const unsigned char base64_table[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0,
    0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

size_t base64_decode(const unsigned char *src, size_t len, unsigned char *out, size_t *out_len)
{
    unsigned char block[4];
    size_t i, count = 0, pad = 0;
    size_t out_idx = 0;

    // Calculate output length
    for (i = 0; i < len; i++)
    {
        if (src[i] == '=')
            pad++;
        if (base64_table[(int)src[i]] != 0 || src[i] == 'A' || src[i] == '=')
        {
            count++;
        }
    }

    *out_len = (count * 3) / 4 - pad;
    if (out == NULL)
    {
        return *out_len;
    }

    // Decode
    for (i = 0; i < len; i += 4)
    {
        block[0] = (i < len) ? base64_table[(int)src[i]] : 0;
        block[1] = (i + 1 < len) ? base64_table[(int)src[i + 1]] : 0;
        block[2] = (i + 2 < len) ? base64_table[(int)src[i + 2]] : 0;
        block[3] = (i + 3 < len) ? base64_table[(int)src[i + 3]] : 0;

        if (out_idx < *out_len)
            out[out_idx++] = (block[0] << 2) | (block[1] >> 4);
        if (out_idx < *out_len)
            out[out_idx++] = ((block[1] & 0x0F) << 4) | (block[2] >> 2);
        if (out_idx < *out_len)
            out[out_idx++] = ((block[2] & 0x03) << 6) | block[3];
    }

    return out_idx;
}

uint8_t *base64_decode_alloc(const char *src, size_t *out_len)
{
    size_t input_len = strlen(src);
    size_t max_output_len = (input_len * 3) / 4 + 1;

    uint8_t *output = malloc(max_output_len);
    if (output == NULL)
    {
        *out_len = 0;
        return NULL;
    }

    size_t actual_len;
    base64_decode((const unsigned char *)src, input_len, output, &actual_len);

    *out_len = actual_len;
    return output;
}