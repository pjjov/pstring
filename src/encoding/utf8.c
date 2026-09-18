/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h" /* IWYU pragma: keep */

char *pstr_write_utf8(char *out, uint32_t c) {
    if (!out || c > 0x10FFFF)
        return NULL;

    if (c <= 0x7F) {
        *out++ = (char)c;
    } else if (c <= 0x7FF) {
        *out++ = (char)(((c >> 6) & 0x1F) | 0xC0);
        *out++ = (char)(((c >> 0) & 0x3F) | 0x80);
    } else if (c <= 0xFFFF) {
        *out++ = (char)(((c >> 12) & 0x0F) | 0xE0);
        *out++ = (char)(((c >> 6) & 0x3F) | 0x80);
        *out++ = (char)(((c >> 0) & 0x3F) | 0x80);
    } else if (c <= 0x10FFFF) {
        *out++ = (char)(((c >> 18) & 0x07) | 0xF0);
        *out++ = (char)(((c >> 12) & 0x3F) | 0x80);
        *out++ = (char)(((c >> 6) & 0x3F) | 0x80);
        *out++ = (char)(((c >> 0) & 0x3F) | 0x80);
    }

    return out;
}

int pstrenc_utf8(pstring_t *dst, const uint32_t *src, size_t length) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, length * 4))
        return PSTRTHROW_ENOMEM;

    char *out = pstrend(dst);
    for (size_t i = 0; i < length; i++)
        out = pstr_write_utf8(out, src[i]);

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}

static int utf8_length(char c) {
    if ((c & 0xF8) == 0xF0)
        return 4; /* 4-byte character, mask = 0x07, shift = 18 */
    else if ((c & 0xF0) == 0xE0)
        return 3; /* 3-byte character, mask = 0x0F, shift = 12 */
    else if ((c & 0xE0) == 0xC0)
        return 2; /* 2-byte character, mask = 0x1F, shift = 6  */
    else if (!(c & 0x80))
        return 1;
    else
        return 0;
}

const char *pstr_read_utf8(const char *chr, const char *end, uint32_t *out) {
    if (!chr || !end || !out || (chr >= end))
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    int left = utf8_length(*chr);

    if (left < 2) {
        /* ASCII character or invalid first byte */
        if (out)
            *out = left == 1 ? *chr : 0xFFFD;
        return chr + 1;
    }

    static uint32_t overlong[4] = { 0, 0x80, 0x800, 0x10000 };

    uint32_t code = 0;
    uint32_t min = overlong[left - 1];
    char mask = (1 << (7 - left)) - 1;
    char shift = (left - 1) * 6;

    for (; chr < end && left > 0; chr++) {
        if (mask == 0x3f && (*chr & 0xC0) != 0x80) {
            /* expected continuation byte */
            code = 0xFFFD;
            break;
        }

        code |= (*chr & mask) << shift;
        mask = 0x3F;
        shift -= 6;
        left--;
    }

    /* overlong encoding */
    if (code < min || left > 0)
        code = 0xFFFD;

    if (out)
        *out = code;
    return chr;
}

int pstrdec_utf8(uint32_t *dst, size_t *length, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    const char *chr = pstrbuf(src);
    const char *end = pstrend(src);
    size_t count = 0, max = *length;

    for (; chr < end && count < max; count++)
        chr = pstr_read_utf8(chr, end, &dst[count]);

    *length = count;
    if (chr < end)
        return PSTRTHROW_ENOMEM;

    return PSTRING_OK;
}
