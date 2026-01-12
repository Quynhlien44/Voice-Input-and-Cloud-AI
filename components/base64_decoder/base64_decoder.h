#ifndef BASE64_DECODER_H
#define BASE64_DECODER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    size_t base64_decode(const unsigned char *src, size_t len, unsigned char *out, size_t *out_len);
    uint8_t *base64_decode_alloc(const char *src, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif