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

#ifndef PSTRING_ENCODING
#define PSTRING_ENCODING

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
typedef struct pstring_t pstring_t;

/** ## NAME

    **pstring-encoding** - encoding and decoding functions for **pstrings**.

    ## DESCRIPTION

    The **pstring** library supports encoding and decoding many common
    formats out of the box. Since strings are assumed to be UTF-8 encoded,
    the encoding functions are divided into two groups:

    - pstrenc - for encoding UTF-8 to a particular format.
    - pstrdec - for decoding from a particular format to UTF-8.

    Both of those groups use the same function prototype, where the contents
    of parameter `src` are encoded and concatenated to parameter `dst`:

    ```c
    int pstrenc_*(pstring_t *dst, const pstring_t *src);
    ```

    This way of handling parameters allows for encoding a stream of data.

    Many operations on pstrings are encoding-agnostic, while others assume
    an ASCII or UTF-8 encoding. For those, UTF-16 support is not explicitly
    supported and pstrings need to be converted to UTF-8 for them to work.

    [TOC]

    ## REFERENCE
**/

typedef int(pstrenc_fn)(pstring_t *dst, const pstring_t *src);

/** Converts `src` into the requested encoding format.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc(pstring_t *dst, const pstring_t *src, const char *enc);

/** Converts `src` from the given encoding format into UTF-8.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec(pstring_t *dst, const pstring_t *src, const char *enc);

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

/** Encodes/decodes RFC 4648 base32 (the `A-Z2-7` alphabet, `=` padding).
    Unlike base64, base32's alphabet avoids visually ambiguous characters
    (no `0`/`O`, `1`/`I`/`l`) and is case-insensitive on decode, which is
    why it shows up in things meant to be typed by hand: TOTP/2FA secret
    keys, DNSSEC/DANE record encodings, and Crockford-style IDs (though
    Crockford's own variant uses a different alphabet, not this one).
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_base32(pstring_t *dst, const pstring_t *src);
PSTR_API int pstrdec_base32(pstring_t *dst, const pstring_t *src);

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

enum pstr_utf16_endian {
    /** Little-endian byte order (the common case: x86/x64, and the
        default assumed by Windows' `wchar_t`/`UTF-16LE`). **/
    PSTR_UTF16_LE = 0,
    /** Big-endian byte order ("UTF-16BE", also the network/file-format
        default when no BOM is present, per RFC 2781). **/
    PSTR_UTF16_BE = 1,
};

/** Encodes codepoints from `src` as UTF-16 code units into `dst`, using
    `endian` byte order. Codepoints above `0xFFFF` are encoded as a
    surrogate pair (four bytes); a codepoint in the surrogate range
    (`0xD800`-`0xDFFF`) or above `0x10FFFF` is invalid UTF-16 and is
    replaced with the replacement character `U+FFFD`, matching how
    `pstrdec_utf8` already handles invalid input rather than failing.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_utf16(
    pstring_t *dst, const uint32_t *src, size_t length, int endian
);

/** Decodes UTF-16 code units from `src` (byte order `endian`) into
    Unicode codepoints in `dst`. `length` should point to the capacity of
    `dst` in codepoints (not code units); it is updated to the number of
    codepoints actually decoded. An unpaired or out-of-order surrogate
    decodes to `U+FFFD` and consumes just the one code unit, so decoding
    never gets permanently stuck on malformed input.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_utf16(
    uint32_t *dst, size_t *length, const pstring_t *src, int endian
);

/** Encodes `src` as a JSON string into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrenc_json(pstring_t *dst, const pstring_t *src);

/** Decodes a JSON string from `src` into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdec_json(pstring_t *dst, const pstring_t *src);

/** Encodes `src` as a XML string into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_ENOENT.
**/
PSTR_API int pstrenc_xml(pstring_t *dst, const pstring_t *src);

/** Decodes a XML string from `src` into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_ENOENT.
**/
PSTR_API int pstrdec_xml(pstring_t *dst, const pstring_t *src);

/** Encodes `src` as a HTML string into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_ENOENT.
**/
PSTR_INLINE int pstrenc_html(pstring_t *dst, const pstring_t *src) {
    return pstrenc_xml(dst, src);
}

/** Decodes a HTML string from `src` into `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_ENOENT.
**/
PSTR_INLINE int pstrdec_html(pstring_t *dst, const pstring_t *src) {
    return pstrdec_xml(dst, src);
}

/** Reads a UTF-8 character and stores it in `out` if it's not `NULL`.
    Returns a pointer to the first byte after the read character.
**/
PSTR_API const char *pstr_read_utf8(
    const char *chr, const char *end, uint32_t *out
);

/** Writes a UTF-8 character and stores it in `out`, returning the
    end of the written byte sequence. The output buffer should
    be big enough to store at least 4 bytes.
**/
PSTR_API char *pstr_write_utf8(char *out, uint32_t c);

/** Reads one UTF-16 code unit sequence (one code unit, or a surrogate
    pair) starting at `chr` and stores the decoded codepoint in `out` if
    it's not `NULL`. Returns a pointer to the first byte after the
    sequence read, which is always `chr + 2` (a lone/invalid code unit)
    or `chr + 4` (a valid surrogate pair). **/
PSTR_API const char *pstr_read_utf16(
    const char *chr, const char *end, uint32_t *out, int endian
);

/** Writes codepoint `c` as one or two UTF-16 code units (four bytes for
    a surrogate pair), returning the end of the written sequence. The
    output buffer should be big enough to store at least 4 bytes. A
    codepoint that cannot be represented in UTF-16 (in the surrogate
    range, or above `0x10FFFF`) is replaced with `U+FFFD`. **/
PSTR_API char *pstr_write_utf16(char *out, uint32_t c, int endian);

#ifdef __cplusplus
}
#endif

#endif
