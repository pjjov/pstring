/*  pstring - fully-featured string library for C

    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Copyright 2026 Предраг Јовановић

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

static struct {
    const char *name;
    pstrenc_fn *enc;
    pstrenc_fn *dec;
} encodings[] = {
    { "base32", pstrenc_base32, pstrdec_base32 },
    { "base64", pstrenc_base64, pstrdec_base64 },
    { "cstring", pstrenc_cstring, pstrdec_cstring },
    { "hex", pstrenc_hex, pstrdec_hex },
    { "html", pstrenc_html, pstrdec_html },
    { "json", pstrenc_json, pstrdec_json },
    { "url", pstrenc_url, pstrdec_url },
    { "xml", pstrenc_xml, pstrdec_xml },
    { 0 },
};

static int find_encoding(const char *name) {
    for (int i = 0; encodings[i].name; i++)
        if (0 == strcmp(name, encodings[i].name))
            return i;
    return -1;
}

int pstrenc(pstring_t *dst, const pstring_t *src, const char *enc) {
    if (!dst || !src || !enc)
        return PSTRTHROW_EINVAL;

    int i = find_encoding(enc);
    return i != -1 ? encodings[i].enc(dst, src) : PSTRTHROW_ENOSYS;
}

int pstrdec(pstring_t *dst, const pstring_t *src, const char *enc) {
    if (!dst || !src || !enc)
        return PSTRTHROW_EINVAL;

    int i = find_encoding(enc);
    return i != -1 ? encodings[i].dec(dst, src) : PSTRTHROW_ENOSYS;
}
