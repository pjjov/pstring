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

#include <stdint.h>
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

int pstrenc_cstring(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

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
            return PSTRING_ENOMEM;
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

static char *writeutf8(char *out, uint32_t c) {
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

int pstrdec_cstring(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    const char *prev = pstrbuf(src);
    const char *end = pstrend(src);
    const char *match = prev;
    char *out = pstrend(dst);
    pstring_t search;

    if (pstrreserve(dst, pstrlen(src)))
        return PSTRING_ENOMEM;

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
                    if (match[i] < '0' && match[i] > '7')
                        break;
                    code = (code << 3) + match[i] - '0';
                }

                if (code >= 256)
                    return PSTRING_EINVAL;

                *out++ = code;
                prev += i - 2;
                break;
            }

            case 'x': {
                char hi = &match[2] < end ? hex2num(match[2]) : 17;
                char lo = &match[3] < end ? hex2num(match[3]) : 17;
                char bounds = &match[4] < end ? hex2num(match[4]) : 17;

                if (hi > 16 || (lo < 16 && bounds < 16))
                    return PSTRING_EINVAL;

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
                    return PSTRING_EINVAL;

                for (int i = 0; i < length; i += 2) {
                    char hi = hex2num(match[i + 2]);
                    char lo = hex2num(match[i + 3]);
                    if (lo > 16 || hi > 16)
                        return PSTRING_EINVAL;
                    c = (c << 8) | (hi * 16 + lo);
                }

                if ((c < 0xA0 && c != 0x24 && c != 0x40 && c != 0x60)
                    || (c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF) {
                    return PSTRING_EINVAL;
                }

                out = writeutf8(out, c);
                break;
            }
            default:
                return PSTRING_EINVAL;
            }
        }
    }

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}

int pstrenc_utf8(pstring_t *dst, const uint32_t *src, size_t length) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (pstrreserve(dst, length * 4))
        return PSTRING_ENOMEM;

    char *out = pstrend(dst);
    for (size_t i = 0; i < length; i++)
        out = writeutf8(out, src[i]);

    pstr__setlen(dst, out - pstrbuf(dst));
    return PSTRING_OK;
}

int pstrdec_utf8(uint32_t *dst, size_t *length, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    const char *chr = pstrbuf(src);
    const char *end = pstrend(src);
    size_t count = 0, max = *length;

    static uint32_t overlong[4] = { 0, 0x80, 0x800, 0x10000 };

    int left = 0;
    char mask, shift;
    uint32_t code, min;
    for (; chr < end && count < max; chr++) {
        char c = *chr;

        if (left == 0) {
            if (!(c & 0x80)) {
                /* ASCII character */
                dst[count++] = c;
                continue;
            }

            if ((c & 0xF8) == 0xF0)
                left = 4; /* 4-byte character, mask = 0x07, shift = 18 */
            else if ((c & 0xF0) == 0xE0)
                left = 3; /* 3-byte character, mask = 0x0F, shift = 12 */
            else if ((c & 0xE0) == 0xC0)
                left = 2; /* 2-byte character, mask = 0x1F, shift = 6  */
            else
                goto failure;

            code = 0;
            mask = (1 << (7 - left)) - 1;
            shift = (left - 1) * 6;
            min = overlong[left - 1];
        } else if ((c & 0xC0) != 0x80) {
            /* expected continuation byte */
            goto failure;
        }

        code |= (c & mask) << shift;
        mask = 0x3F;
        shift -= 6;

        if (--left == 0) {
#ifndef PSTRING_ALLOW_OVERLONG
            if (code < min)
                goto failure;
#else
            (void)min;
#endif
            dst[count++] = code;
        }

        continue;
    failure:
        /* use a replacement character */
        dst[count++] = 0xFFFD;
        left = 0;
    }

    *length = count;
    if (chr < end)
        return PSTRING_ENOMEM;

    return PSTRING_OK;
}
