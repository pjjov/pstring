/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h"

int pstrenc_url(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRTHROW_ENOMEM;

    const char *hexdigits = HEXDIGITS;

    static pstring_t safe_chars = PSTRWRAP(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_~."
    );

    const char *in = pstrbuf(src);
    size_t length = pstrlen(src);

    char *out = pstrbuf(dst);
    size_t j = pstrlen(dst);

    for (size_t i = 0; i < length; i++) {
        if (!pstrchr(&safe_chars, in[i])) {
            if (pstrreserve(dst, j + 2))
                return PSTRTHROW_ENOMEM;

            out = pstrbuf(dst);
            out[j++] = '%';
            out[j++] = hexdigits[(in[i] & 0xF0) >> 4];
            out[j++] = hexdigits[in[i] & 0x0F];
        } else {
            out[j++] = in[i];
        }
    }

    pstr__setlen(dst, j);
    return PSTRING_OK;
}

int pstrdec_url(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRTHROW_ENOMEM;

    char *out = pstrend(dst);
    const char *end = pstrend(src);
    const char *escape = pstrbuf(src);
    const char *prev = escape;
    pstring_t search;

    for (; &escape[2] < end; prev = escape) {
        pstrrange(&search, NULL, escape, end);

        if (!(escape = pstrchr(&search, '%')))
            escape = end;

        memcpy(out, prev, escape - prev);
        out += escape - prev;

        char hi = hex2num(escape[1]);
        char lo = hex2num(escape[2]);
        if (hi > 16 || lo > 16)
            return PSTRTHROW_EDECODE;
        *out++ = hi * 16 + lo;
        escape += 3;
    }

    /* % at the end of the string */
    if (escape < end) {
        memcpy(out, prev, end - escape);
        out += end - escape;
    }

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}