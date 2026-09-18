/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h" /* IWYU pragma: keep */

int pstrenc_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *_table
) {
    if (!dst || !src || !_table || pstrlen(_table) != 64)
        return PSTRTHROW_EINVAL;

    size_t len = pstrlen(src);
    if (pstrreserve(dst, (len / 3 + 1) * 4))
        return PSTRTHROW_ENOMEM;

    char *out = pstrend(dst);
    const char *chr = pstrbuf(src);
    const char *end = pstrend(src);
    const char *table = pstrbuf(_table);

    for (; &chr[2] < end; chr += 3) {
        out[0] = table[(chr[0] & 0xFC) >> 2];
        out[1] = table[((chr[0] & 0x03) << 4) | ((chr[1] & 0xF0) >> 4)];
        out[2] = table[((chr[1] & 0x0F) << 2) | ((chr[2] & 0xC0) >> 6)];
        out[3] = table[chr[2] & 0x3F];
        out += 4;
    }

    if (&chr[1] < end) {
        out[0] = table[(chr[0] & 0xFC) >> 2];
        out[1] = table[((chr[0] & 0x03) << 4) | ((chr[1] & 0xF0) >> 4)];
        out[2] = table[((chr[1] & 0x0F) << 2)];
        out[3] = '=';
        out += 4;
    } else if (chr < end) {
        out[0] = table[(chr[0] & 0xFC) >> 2];
        out[1] = table[((chr[0] & 0x03) << 4)];
        out[2] = out[3] = '=';
        out += 4;
    }

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}

static char base2num(const pstring_t *table, char chr) {
    char *match = pstrchr(table, chr);
    return match ? match - pstrbuf(table) : pstrlen(table);
}

int pstrdec_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *table
) {
    if (!dst || !src || !table || pstrlen(table) != 64)
        return PSTRTHROW_EINVAL;

    size_t len = pstrlen(src);

    /* an empty input decodes to an empty output; without this, `end[-1]`
       below reads one byte before the (empty) buffer */
    if (len == 0)
        return PSTRING_OK;

    if (pstrreserve(dst, (len / 4 + 1) * 3))
        return PSTRTHROW_ENOMEM;

    char *out = pstrend(dst);
    const char *chr = pstrbuf(src);
    const char *end = pstrend(src);
    char v[4];

    /* padding characters */
    if (end[-1] == '=')
        end--;
    if (end > chr && end[-1] == '=')
        end--;

    for (; &chr[3] < end; chr += 4) {
        v[0] = base2num(table, chr[0]);
        v[1] = base2num(table, chr[1]);
        v[2] = base2num(table, chr[2]);
        v[3] = base2num(table, chr[3]);

        if (v[0] > 64 || v[1] > 64 || v[2] > 64 || v[3] > 64)
            return PSTRTHROW_EDECODE;

        *out++ = (v[0] << 2) | ((v[1] & 0x30) >> 4);
        *out++ = ((v[1] & 0x0F) << 4) | ((v[2] & 0x3C) >> 2);
        *out++ = ((v[2] & 0x03) << 6) | v[3];
    }

    if (&chr[2] < end) {
        v[0] = base2num(table, chr[0]);
        v[1] = base2num(table, chr[1]);
        v[2] = base2num(table, chr[2]);

        if (v[0] > 64 || v[1] > 64 || v[2] > 64)
            return PSTRTHROW_EDECODE;

        *out++ = (v[0] << 2) | ((v[1] & 0x30) >> 4);
        *out++ = ((v[1] & 0x0F) << 4) | ((v[2] & 0x3C) >> 2);
    } else if (&chr[1] < end) {
        v[0] = base2num(table, chr[0]);
        v[1] = base2num(table, chr[1]);

        if (v[0] > 64 || v[1] > 64)
            return PSTRTHROW_EDECODE;

        *out++ = (v[0] << 2) | ((v[1] & 0x30) >> 4);
    }

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}

static const pstring_t base64_table = PSTRWRAP(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/"
);

static const pstring_t base64url_table = PSTRWRAP(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_"
);

int pstrenc_base64(pstring_t *dst, const pstring_t *src) {
    return pstrenc_base64table(dst, src, &base64_table);
}

int pstrenc_base64url(pstring_t *dst, const pstring_t *src) {
    return pstrenc_base64table(dst, src, &base64url_table);
}

int pstrdec_base64(pstring_t *dst, const pstring_t *src) {
    return pstrdec_base64table(dst, src, &base64_table);
}

int pstrdec_base64url(pstring_t *dst, const pstring_t *src) {
    return pstrdec_base64table(dst, src, &base64url_table);
}
