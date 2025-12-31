/*  pstring - String library for C with SIMD acceleration

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

#ifndef PSTRING_ENCODING
#define PSTRING_ENCODING

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#include <stddef.h>
#include <stdint.h>
typedef struct pstring_t pstring_t;

/** Encodes the bytes from `src` as a string of hexadecimal numbers.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_hex(pstring_t *dst, const pstring_t *src);

/** Decodes a series of hexadecimal numbers from `src` into a series of bytes.
    The source string should have an even length and contain only hexadecimal
    digits ('0123456789abcdefABCDEF').
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_hex(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a URL compatible string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_url(pstring_t *dst, const pstring_t *src);

/** Decodes a URL-encoded string from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_url(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a Base64-encoded string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_base64(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a URL-safe Base64-encoded string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_base64url(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a Base64-encoded string using the provided
    translation table. `table` must be exactly 64 characters long.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *table
);

/** Decodes a Base64-encoded string from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_base64(pstring_t *dst, const pstring_t *src);

/** Decodes a URL-safe Base64-encoded string from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_base64url(pstring_t *dst, const pstring_t *src);

/** Decodes a Base64-encoded string from `src` using the provided
    translation table. `table` must be exactly 64 characters long.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *table
);

/** Stores an escaped version of `src` into `dst`, which, if surrounded
    by quotes, is safe to use as a string literal in C source code.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_cstring(pstring_t *dst, const pstring_t *src);

/** Expands C escape sequences found in `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_cstring(pstring_t *dst, const pstring_t *src);

/** Encodes codepoints from `src` as UTF8 characters.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_utf8(pstring_t *dst, const uint32_t *src, size_t length);

/** Decodes UTF-8 characters from `src` as Unicode codepoints.
    Parameter `length` should point to the maximum length of the buffer `dst`,
    which will be changed by the function to the number of codepoints decoded.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_utf8(uint32_t *dst, size_t *length, const pstring_t *src);

/** Encodes `src` as a JSON string into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_json(pstring_t *dst, const pstring_t *src);

/** Decodes a JSON string from `src` into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_json(pstring_t *dst, const pstring_t *src);

/** Encodes `src` as a XML string into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_xml(pstring_t *dst, const pstring_t *src);

/** Decodes a XML string from `src` into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_xml(pstring_t *dst, const pstring_t *src);

/** Encodes `src` as a HTML string into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_INLINE int pstrenc_html(pstring_t *dst, const pstring_t *src) {
    return pstrenc_xml(dst, src);
}

/** Decodes a HTML string from `src` into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_INLINE int pstrdec_html(pstring_t *dst, const pstring_t *src) {
    return pstrdec_xml(dst, src);
}

#endif
