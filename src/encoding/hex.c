/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h"

int pstrenc_hex(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, pstrlen(src) * 2))
        return PSTRTHROW_ENOMEM;

    const char *hexdigits = HEXDIGITS;

    char *in = pstrbuf(src);
    char *end = pstrend(src);
    char *out = pstrend(dst);

    while (in < end) {
        *out++ = hexdigits[*in / 16];
        *out++ = hexdigits[*in % 16];
        in++;
    }

    pstr__setlen(dst, pstrlen(dst) + pstrlen(src) * 2);
    return PSTRING_OK;
}

int pstrdec_hex(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrlen(src) % 2 != 0)
        return PSTRTHROW_EDECODE;

    if (pstrreserve(dst, pstrlen(src) / 2))
        return PSTRTHROW_ENOMEM;

    char *in = pstrbuf(src);
    char *end = pstrend(src);
    char *out = pstrend(dst);

    while (&in[1] < end) {
        char hi = hex2num(*in++);
        char lo = hex2num(*in++);
        if (hi > 16 || lo > 16)
            return PSTRTHROW_EDECODE;

        *out++ = hi * 16 + lo;
    }

    pstr__setlen(dst, pstrlen(src) / 2);
    return PSTRING_OK;
}
