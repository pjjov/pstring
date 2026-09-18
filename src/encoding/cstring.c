/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h"

int pstrenc_cstring(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    const char *prev = pstrbuf(src);
    const char *end = pstrend(src);
    const char *match = prev;
    pstring_t search;

    size_t dstlen = pstrlen(dst);
    size_t index = dstlen;

    while (match) {
        pstrrange(&search, NULL, prev, end);

        match = pstrcpbrk(
            &search,
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
            " !#%&()*+,-./0123456789:;<=>[]^_{|}~"
        );

        size_t length = match ? match - prev : end - prev;

        if (pstrreserve(dst, length + 4)) {
            pstr__setlen(dst, dstlen);
            return PSTRTHROW_ENOMEM;
        }

        char *out = pstrbuf(dst);
        memcpy(&out[index], prev, length);
        index += length;

        if (match) {
            out[index++] = '\\';

            switch (*match) {
                /* clang-format off */
            case '\?': out[index++] = '?'; break;
            case '\'': out[index++] = '\''; break;
            case '\"': out[index++] = '\"'; break;
            case '\\': out[index++] = '\\'; break;
            case '\a': out[index++] = 'a'; break;
            case '\b': out[index++] = 'b'; break;
            case '\f': out[index++] = 'f'; break;
            case '\n': out[index++] = 'n'; break;
            case '\r': out[index++] = 'r'; break;
            case '\t': out[index++] = 't'; break;
            case '\v': out[index++] = 'v'; break;
                /* clang-format on */

            default:
                out[index++] = (*match >> 6) + '0';
                out[index++] = ((*match >> 3) & 7) + '0';
                out[index++] = (*match & 7) + '0';
                break;
            }

            prev = match + 1;
        }

        pstr__setlen(dst, index);
    }

    return PSTRING_OK;
}

int pstrdec_cstring(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    const char *prev = pstrbuf(src);
    const char *end = pstrend(src);
    const char *match = prev;
    char *out = pstrend(dst);
    pstring_t search;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRTHROW_ENOMEM;

    while (match) {
        pstrrange(&search, NULL, prev, end - 1);
        match = pstrchr(&search, '\\');
        size_t length = match ? match - prev : end - prev;

        memcpy(out, prev, length);
        prev = match + 2;
        out += length;

        if (match) {
            switch (match[1]) {
                /* clang-format off */
            case '?':  *out++ = '\?'; break;
            case '\'': *out++ = '\''; break;
            case '\"': *out++ = '\"'; break;
            case '\\': *out++ = '\\'; break;
            case 'a':  *out++ = '\a'; break;
            case 'b':  *out++ = '\b'; break;
            case 'f':  *out++ = '\f'; break;
            case 'n':  *out++ = '\n'; break;
            case 'r':  *out++ = '\r'; break;
            case 't':  *out++ = '\t'; break;
            case 'v':  *out++ = '\v'; break;

            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                /* clang-format on */
                unsigned long code = 0;
                size_t i = 1;

                for (; i < 4 && &match[i] < end; i++) {
                    if (match[i] < '0' || match[i] > '7')
                        break;
                    code = (code << 3) + match[i] - '0';
                }

                if (code >= 256)
                    return PSTRTHROW_EDECODE;

                *out++ = code;
                prev += i - 2;
                break;
            }

            case 'x': {
                char hi = &match[2] < end ? hex2num(match[2]) : 17;
                char lo = &match[3] < end ? hex2num(match[3]) : 17;
                char bounds = &match[4] < end ? hex2num(match[4]) : 17;

                if (hi > 16 || (lo < 16 && bounds < 16))
                    return PSTRTHROW_EDECODE;

                if (lo < 16) {
                    *out++ = hi * 16 + lo;
                    prev += 2;
                } else {
                    *out++ = hi;
                    prev++;
                }

                break;
            }

            case 'u':
            case 'U': {
                uint32_t c = 0;
                int length = match[1] == 'U' ? 8 : 4;
                prev += length;

                if (&match[length + 1] >= end)
                    return PSTRTHROW_EDECODE;

                for (int i = 0; i < length; i += 2) {
                    char hi = hex2num(match[i + 2]);
                    char lo = hex2num(match[i + 3]);
                    if (lo > 16 || hi > 16)
                        return PSTRTHROW_EDECODE;
                    c = (c << 8) | (hi * 16 + lo);
                }

                if ((c < 0xA0 && c != 0x24 && c != 0x40 && c != 0x60)
                    || (c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF) {
                    return PSTRTHROW_EDECODE;
                }

                out = pstr_write_utf8(out, c);
                break;
            }
            default:
                return PSTRTHROW_EDECODE;
            }
        }
    }

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}