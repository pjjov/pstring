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

int pstrenc_hex(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (pstrreserve(dst, pstrlen(src) * 2))
        return PSTRING_ENOMEM;

    char *in = pstrbuf(src);
    char *end = pstrend(src);
    char *out = pstrend(dst);
    static const char *hexdigits = "0123456789ABCDEF";

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
