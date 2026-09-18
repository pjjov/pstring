/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h"

static inline int is_json_esc(char c) {
    return c < 0x20 || c == '/' || c == '\\' || c == '"';
}

int pstrenc_json(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRTHROW_ENOMEM;

    const char *hexdigits = HEXDIGITS;
    const char *curr = pstrbuf(src);
    const char *end = pstrend(src);
    size_t i;

    for (i = 0; curr < end; i++, curr++) {
        if (pstrreserve(dst, i + 6))
            return PSTRTHROW_ENOMEM;

        char *out = pstrend(dst);
        if (is_json_esc(*curr))
            out[i++] = '\\';

        switch (*curr) {
            /* clang-format off */
        case '\b': out[i] = 'b'; break;
        case '\f': out[i] = 'f'; break;
        case '\n': out[i] = 'n'; break;
        case '\r': out[i] = 'r'; break;
        case '\t': out[i] = 't'; break;
            /* clang-format on */
        default:
            if (*curr < 0x20) {
                memcpy(&out[i], "u00", 3);
                out[i + 3] = hexdigits[(*curr >> 4) & 0xF];
                out[i + 4] = hexdigits[(*curr) & 0xF];
                i += 4;
            } else {
                out[i] = *curr;
            }

            break;
        }
    }

    pstr__setlen(dst, pstrlen(dst) + i);
    return PSTRING_OK;
}

int pstrdec_json(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRTHROW_EINVAL;

    pstring_t search;
    pstrwrap(&search, pstrbuf(src), pstrlen(src), 0);

    const char *match, *prev = pstrbuf(src);
    const char *end = pstrend(src);
    char *out = pstrend(dst);

    while ((match = pstrchr(&search, '\\'))) {
        memcpy(out, prev, match - prev);
        out += match - prev;
        prev = match + 2;

        if (&match[1] >= end)
            return PSTRTHROW_EDECODE;

        switch (match[1]) {
            /* clang-format off */
        case '\\': *out++ = '\\'; break;
        case '\'': *out++ = '\''; break;
        case '/':  *out++ = '/';  break;
        case '"':  *out++ = '"';  break;
        case 'b':  *out++ = '\b'; break;
        case 'f':  *out++ = '\f'; break;
        case 'n':  *out++ = '\n'; break;
        case 'r':  *out++ = '\r'; break;
        case 't':  *out++ = '\t'; break;
            /* clang-format on */

        case 'u': {
            if (&match[5] >= end)
                return PSTRTHROW_EDECODE;

            uint32_t code = 0;
            uint8_t hex;

            for (int i = 0; i < 4; i++) {
                if ((hex = hex2num(match[i + 2])) >= 16)
                    return PSTRTHROW_EDECODE;

                code = (code << 4) | hex;
            }

            out = pstr_write_utf8(out, code);
            prev = match + 6;
            break;
        }

        default:
            return PSTRTHROW_EDECODE;
        }

        pstrrange(&search, NULL, prev, pstrend(src));
    }

    memcpy(out, prev, end - prev);
    out += end - prev;

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}