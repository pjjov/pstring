/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h" /* IWYU pragma: keep */

static inline uint16_t utf16_get(const char *p, int endian) {
    unsigned char a = (unsigned char)p[0], b = (unsigned char)p[1];
    return endian == PSTR_UTF16_BE ? (uint16_t)((a << 8) | b)
                                   : (uint16_t)((b << 8) | a);
}

static inline void utf16_put(char *p, uint16_t v, int endian) {
    if (endian == PSTR_UTF16_BE) {
        p[0] = (char)(v >> 8);
        p[1] = (char)(v & 0xFF);
    } else {
        p[0] = (char)(v & 0xFF);
        p[1] = (char)(v >> 8);
    }
}

char *pstr_write_utf16(char *out, uint32_t c, int endian) {
    if (!out)
        return NULL;

    if ((c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)
        c = 0xFFFD; /* not representable in UTF-16 */

    if (c <= 0xFFFF) {
        utf16_put(out, (uint16_t)c, endian);
        return out + 2;
    }

    /* surrogate pair: split the 20-bit offset above the BMP across a
       high surrogate (top 10 bits) and low surrogate (bottom 10 bits) */
    uint32_t offset = c - 0x10000;
    uint16_t hi = (uint16_t)(0xD800 + (offset >> 10));
    uint16_t lo = (uint16_t)(0xDC00 + (offset & 0x3FF));

    utf16_put(out, hi, endian);
    utf16_put(out + 2, lo, endian);
    return out + 4;
}

int pstrenc_utf16(
    pstring_t *dst, const uint32_t *src, size_t length, int endian
) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    /* worst case: every codepoint needs a surrogate pair, 4 bytes each */
    if (pstrreserve(dst, length * 4))
        return PSTRTHROW_ENOMEM;

    char *out = pstrend(dst);
    for (size_t i = 0; i < length; i++)
        out = pstr_write_utf16(out, src[i], endian);

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}

const char *pstr_read_utf16(
    const char *chr, const char *end, uint32_t *out, int endian
) {
    if (!chr || !end || (chr >= end))
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    /* a lone trailing byte is not a valid code unit at all */
    if (end - chr < 2) {
        if (out)
            *out = 0xFFFD;
        return chr + 1;
    }

    uint16_t unit = utf16_get(chr, endian);

    if (unit < 0xD800 || unit > 0xDFFF) {
        /* ordinary code unit, one-to-one with its codepoint */
        if (out)
            *out = unit;
        return chr + 2;
    }

    if (unit <= 0xDBFF) {
        /* high surrogate: needs a following low surrogate to complete
           the pair. Any failure to find one (truncated input, or a
           high surrogate followed by something that isn't a low
           surrogate) decodes to U+FFFD and consumes only the high
           surrogate itself, so a scan can resynchronise on the next
           code unit instead of getting stuck. */
        if (end - chr < 4) {
            if (out)
                *out = 0xFFFD;
            return chr + 2;
        }

        uint16_t low = utf16_get(chr + 2, endian);
        if (low < 0xDC00 || low > 0xDFFF) {
            if (out)
                *out = 0xFFFD;
            return chr + 2;
        }

        if (out)
            *out = 0x10000 + (((uint32_t)unit - 0xD800) << 10) + (low - 0xDC00);
        return chr + 4;
    }

    /* a low surrogate with no preceding high surrogate */
    if (out)
        *out = 0xFFFD;
    return chr + 2;
}

int pstrdec_utf16(
    uint32_t *dst, size_t *length, const pstring_t *src, int endian
) {
    if (!dst || !src || !length)
        return PSTRTHROW_EINVAL;

    const char *chr = pstrbuf(src);
    const char *end = pstrend(src);
    size_t count = 0, max = *length;

    for (; chr < end && count < max; count++)
        chr = pstr_read_utf16(chr, end, &dst[count], endian);

    *length = count;
    if (chr < end)
        return PSTRTHROW_ENOMEM;

    return PSTRING_OK;
}