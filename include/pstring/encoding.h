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

typedef struct pstring_t pstring_t;

/** Encodes the bytes from `src` as a string of hexadecimal numbers.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrenc_hex(pstring_t *dst, const pstring_t *src);

/** Decodes a series of hexadecimal numbers from `src` into a series of bytes.
    The source string should have an even length and contain only hexadecimal
    digits ('0123456789abcdefABCDEF').
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrdec_hex(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a URL compatible string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrenc_url(pstring_t *dst, const pstring_t *src);

/** Decodes a URL-encoded string from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrdec_url(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a Base64-encoded string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrenc_base64(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a URL-safe Base64-encoded string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrenc_base64url(pstring_t *dst, const pstring_t *src);

/** Encodes `src` into a Base64-encoded string using the provided
    translation table. `table` must be exactly 64 characters long.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrenc_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *table
);

/** Decodes a Base64-encoded string from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrdec_base64(pstring_t *dst, const pstring_t *src);

/** Decodes a URL-safe Base64-encoded string from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrdec_base64url(pstring_t *dst, const pstring_t *src);

/** Decodes a Base64-encoded string from `src` using the provided
    translation table. `table` must be exactly 64 characters long.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrdec_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *table
);

#endif
