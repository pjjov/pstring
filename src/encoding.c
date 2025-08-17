/*
    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <pstring/encoding.h>
#include <pstring/pstring.h>

#include <string.h>

static const char *hexdigits = "0123456789ABCDEF";

int pstrenc_hex(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (pstrreserve(dst, pstrlen(src) * 2))
        return PSTRING_ENOMEM;

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

static inline char hex2num(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 17;
}

int pstrdec_hex(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src || pstrlen(src) % 2 != 0)
        return PSTRING_EINVAL;

    if (pstrreserve(dst, pstrlen(src) / 2))
        return PSTRING_ENOMEM;

    char *in = pstrbuf(src);
    char *end = pstrend(src);
    char *out = pstrend(dst);

    while (&in[1] < end) {
        char hi = hex2num(*in++);
        char lo = hex2num(*in++);
        if (hi > 16 || lo > 16)
            return PSTRING_EINVAL;

        *out++ = hi * 16 + lo;
    }

    pstr__setlen(dst, pstrlen(src) / 2);
    return PSTRING_OK;
}

int pstrenc_url(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRING_ENOMEM;

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
                return PSTRING_ENOMEM;

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
        return PSTRING_EINVAL;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRING_ENOMEM;

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
            return PSTRING_EINVAL;
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

int pstrenc_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *_table
) {
    if (!dst || !src || !_table || pstrlen(_table) != 64)
        return PSTRING_EINVAL;

    size_t len = pstrlen(src);
    if (pstrreserve(dst, (len / 3 + 1) * 4))
        return PSTRING_ENOMEM;

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
        return PSTRING_EINVAL;

    size_t len = pstrlen(src);
    if (pstrreserve(dst, (len / 4 + 1) * 3))
        return PSTRING_ENOMEM;

    char *out = pstrend(dst);
    const char *chr = pstrbuf(src);
    const char *end = pstrend(src);
    char v[4];

    /* padding characters */
    if (end[-1] == '=')
        end--;
    if (end[-1] == '=')
        end--;

    for (; &chr[3] < end; chr += 4) {
        v[0] = base2num(table, chr[0]);
        v[1] = base2num(table, chr[1]);
        v[2] = base2num(table, chr[2]);
        v[3] = base2num(table, chr[3]);

        if (v[0] > 64 || v[1] > 64 || v[2] > 64 || v[3] > 64)
            return PSTRING_EINVAL;

        *out++ = (v[0] << 2) | ((v[1] & 0x30) >> 4);
        *out++ = ((v[1] & 0x0F) << 4) | ((v[2] & 0x3C) >> 2);
        *out++ = ((v[2] & 0x03) << 6) | v[3];
    }

    if (&chr[2] < end) {
        v[0] = base2num(table, chr[0]);
        v[1] = base2num(table, chr[1]);
        v[2] = base2num(table, chr[2]);

        if (v[0] > 64 || v[1] > 64 || v[2] > 64)
            return PSTRING_EINVAL;

        *out++ = (v[0] << 2) | ((v[1] & 0x30) >> 4);
        *out++ = ((v[1] & 0x0F) << 4) | ((v[2] & 0x3C) >> 2);
    } else if (&chr[1] < end) {
        v[0] = base2num(table, chr[0]);
        v[1] = base2num(table, chr[1]);

        if (v[0] > 64 || v[1] > 64)
            return PSTRING_EINVAL;

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
