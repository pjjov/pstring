/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "common.h"

int pstrenc_xml(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRTHROW_ENOMEM;

    pstring_t search;
    pstrwrap(&search, pstrbuf(src), pstrlen(src), 0);

    const char *match, *prev = pstrbuf(src);
    const char *end = pstrend(src);
    size_t i = 0;

    while ((match = pstrpbrk(&search, "<>&\"'"))) {
        if (pstrreserve(dst, i + match - prev + 6))
            return PSTRTHROW_ENOMEM;

        char *out = pstrend(dst);
        memcpy(&out[i], prev, match - prev);
        i += match - prev;

        switch (*match) {
            /* clang-format off */
        case '<':  memcpy(&out[i], "&lt;", 4);   i += 4; break;
        case '>':  memcpy(&out[i], "&gt;", 4);   i += 4; break;
        case '&':  memcpy(&out[i], "&amp;", 5);  i += 5; break;
        case '"':  memcpy(&out[i], "&quot;", 6); i += 6; break;
        case '\'': memcpy(&out[i], "&#39;", 5);  i += 5; break;
            /* clang-format on */
        }

        prev = match + 1;
        pstrrange(&search, NULL, prev, end);
    }

    memcpy(&pstrend(dst)[i], prev, end - prev);
    pstr__setlen(dst, pstrlen(dst) + i + (end - prev));
    return PSTRING_OK;
}

static inline int decode_xml_hex(char *out, pstring_t *entity) {
    uint32_t code = 0;
    uint8_t hex;

    for (int i = 0; i < 4; i++) {
        if (16 >= (hex = hex2num(pstrget(entity, i + 1))))
            break;

        code = (code << 4) | hex;
    }

    return pstr_write_utf8(out, code) - out;
}

static inline int decode_xml_dec(char *out, pstring_t *entity) {
    uint32_t code = 0;
    uint8_t dec;

    for (int i = 0; i < 4; i++) {
        if (10 >= (dec = dec2num(pstrget(entity, i + 1))))
            break;

        code = (code * 10) + dec;
    }

    return pstr_write_utf8(out, code) - out;
}

static inline int decode_xml(char *out, pstring_t *entity) {
    if (pstrget(entity, 0) == '#') {
        if (pstrget(entity, 1) == 'x')
            return decode_xml_hex(out, entity);
        else
            return decode_xml_dec(out, entity);
    }

    char *result = NULL;

#define DECODE_XML(code, num)               \
    if (pstrequal(entity, PSTR(code)))      \
        result = pstr_write_utf8(out, num);

    DECODE_XML("amp", '&');
    DECODE_XML("lt", '<');
    DECODE_XML("gt", '>');
    DECODE_XML("quot", '"');
    return result ? result - out : PSTRING_ENOENT;
}

int pstrdec_xml(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRTHROW_ENOMEM;

    pstring_t search;
    pstrwrap(&search, pstrbuf(src), pstrlen(src), 0);

    const char *amp, *semi;
    const char *prev = pstrbuf(src);
    const char *end = pstrend(src);
    size_t i = 0, shift;

    while ((amp = pstrchr(&search, '&'))) {
        if (pstrreserve(dst, i + amp - prev + 6))
            return PSTRTHROW_ENOMEM;

        /* copy unescaped chunk */
        char *out = pstrend(dst);
        memcpy(&out[i], prev, amp - prev);
        i += amp - prev;

        /* find semicolon */
        pstrrange(&search, NULL, amp, end);
        if (!(semi = pstrchr(&search, ';')))
            return PSTRTHROW_EDECODE;

        /* decode character entity */
        pstrrange(&search, NULL, amp + 1, semi);
        if ((shift = decode_xml(&out[i], &search)) < 0)
            return PSTRTHROW_EDECODE;

        i += shift;
        prev = semi + 1;
        pstrrange(&search, NULL, prev, end);
    }

    memcpy(&pstrend(dst)[i], prev, end - prev);
    pstr__setlen(dst, pstrlen(dst) + i + (end - prev));
    return PSTRING_OK;
}
