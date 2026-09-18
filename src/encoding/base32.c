/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h" /* IWYU pragma: keep */

static const char base32_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int pstrenc_base32(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    size_t len = pstrlen(src);
    if (len == 0)
        return PSTRING_OK;

    /* 5 input bytes (40 bits) become 8 output characters (5 bits each) */
    if (pstrreserve(dst, ((len + 4) / 5) * 8))
        return PSTRTHROW_ENOMEM;

    const unsigned char *in = (const unsigned char *)pstrbuf(src);
    char *out = pstrend(dst);
    size_t i = 0;

    for (; i + 5 <= len; i += 5, in += 5) {
        uint64_t buf = in[4];
        buf |= ((uint64_t)in[0] << 32);
        buf |= ((uint64_t)in[1] << 24);
        buf |= ((uint64_t)in[2] << 16);
        buf |= ((uint64_t)in[3] << 8);

        for (int shift = 35; shift >= 0; shift -= 5)
            *out++ = base32_alphabet[(buf >> shift) & 0x1F];
    }

    size_t rem = len - i;
    if (rem > 0) {
        unsigned char tail[5] = { 0, 0, 0, 0, 0 };
        memcpy(tail, in, rem);

        uint64_t buf = tail[4];
        buf |= ((uint64_t)tail[0] << 32);
        buf |= ((uint64_t)tail[1] << 24);
        buf |= ((uint64_t)tail[2] << 16);
        buf |= ((uint64_t)tail[3] << 8);

        /* how many of the 8 output characters carry real data, per
           RFC 4648 sec. 6's padding table (1/2/3/4 leftover bytes need
           2/4/5/7 characters respectively; the rest is '=' padding) */
        static const int usedChars[] = { 0, 2, 4, 5, 7 };
        int used = usedChars[rem];

        for (int j = 0, shift = 35; j < used; j++, shift -= 5)
            *out++ = base32_alphabet[(buf >> shift) & 0x1F];
        for (int j = used; j < 8; j++)
            *out++ = '=';
    }

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}

int pstrdec_base32(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    size_t len = pstrlen(src);
    if (len == 0)
        return PSTRING_OK;

    if (pstrreserve(dst, (len / 8 + 1) * 5))
        return PSTRTHROW_ENOMEM;

    char *out = pstrend(dst);
    const char *chr = pstrbuf(src);
    const char *end = pstrend(src);

    while (end > chr && end[-1] == '=')
        end--;

    uint64_t buf = 0;
    int bits = 0;

    for (; chr < end; chr++) {
        char c = *chr;
        int val;

        if (c >= 'A' && c <= 'Z')
            val = c - 'A';
        else if (c >= 'a' && c <= 'z')
            val = c - 'a'; /* base32 decoding is case-insensitive */
        else if (c >= '2' && c <= '7')
            val = c - '2' + 26;
        else
            return PSTRTHROW_EINVAL; /* not a base32 character */

        buf = (buf << 5) | (unsigned)val;
        bits += 5;

        if (bits >= 8) {
            bits -= 8;
            *out++ = (char)((buf >> bits) & 0xFF);
        }
    }

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}