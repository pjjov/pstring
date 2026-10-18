# Encoding and decoding functions for pstrings.

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

- [`pstrenc_fn`](#pstrenc_fn) — Encoding/decoding function prototype.
- [`pstrenc`](#pstrenc) — Encodes/decodes `src` using the requested encoding format.
- [`pstrenc_hex`](#pstrenc_hex) — Encodes/Decodes the bytes from `src` as a string of hexadecimal numbers.
- [`pstrenc_url`](#pstrenc_url) — Encodes/decodes `src` as a URL compatible string.
- [`pstrenc_base32`](#pstrenc_base32) — Encodes/decodes RFC 4648 base32 (the `A-Z2-7` alphabet, `=` padding).
- [`pstrenc_base64`](#pstrenc_base64) — Encodes/decodes `src` as a Base64-encoded string.
- [`pstrenc_base64url`](#pstrenc_base64url) — Encodes/decodes `src` as a URL-safe Base64-encoded string.
- [`pstrenc_base64table`](#pstrenc_base64table) — Encodes/decodes `src` as a Base64-encoded string using the provided translation table.
- [`pstrenc_cstring`](#pstrenc_cstring) — Encodes/decodes an escaped version of `src` into `dst`, which, if surrounded by quotes, is safe to use as a string literal in C source code.
- [`pstrenc_utf8`](#pstrenc_utf8) — Encodes/decodes codepoints from `src` as UTF8 characters.
- [`pstrenc_utf16`](#pstrenc_utf16) — Encodes codepoints from `src` as UTF-16 code units into `dst`, using `endian` byte order.
- [`pstrdec_utf16`](#pstrdec_utf16) — Decodes UTF-16 code units from `src` (byte order `endian`) into Unicode codepoints in `dst`.
- [`pstrenc_json`](#pstrenc_json) — Encodes/decodes `src` as a JSON string into `dst`.
- [`pstrenc_xml`](#pstrenc_xml) — Encodes/decodes `src` as a XML string into `dst`.
- [`pstrenc_html`](#pstrenc_html) — Encodes/decodes `src` as a HTML string into `dst`.
- [`pstr_read_utf8`](#pstr_read_utf8) — Reads a UTF-8 character and stores it in `out` if it's not `NULL`.
- [`pstr_write_utf8`](#pstr_write_utf8) — Writes a UTF-8 character and stores it in `out`, returning the end of the written byte sequence.
- [`pstr_read_utf16`](#pstr_read_utf16) — Reads one UTF-16 code unit sequence (one code unit, or a surrogate pair) starting at `chr` and stores the decoded codepoint in `out` if it's not `NULL`.
- [`pstr_write_utf16`](#pstr_write_utf16) — Writes codepoint `c` as one or two UTF-16 code units (four bytes for a surrogate pair), returning the end of the written sequence.

<a id="pstrenc_fn"></a>

## `pstrenc_fn`

```c
typedef int(pstrenc_fn)(pstring_t *dst, const pstring_t *src);
```

Encoding/decoding function prototype.

<a id="pstrenc"></a>

## `pstrenc`

```c
int pstrenc(pstring_t *dst, const pstring_t *src, const char *enc);
int pstrdec(pstring_t *dst, const pstring_t *src, const char *enc);
```

Encodes/decodes `src` using the requested encoding format.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_hex"></a>

## `pstrenc_hex`

```c
int pstrenc_hex(pstring_t *dst, const pstring_t *src);
int pstrdec_hex(pstring_t *dst, const pstring_t *src);
```

Encodes/Decodes the bytes from `src` as a string of hexadecimal numbers.

For decoding, the source string should have an even length and contain only
hexadecimal digits ('0123456789abcdefABCDEF').

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_url"></a>

## `pstrenc_url`

```c
int pstrenc_url(pstring_t *dst, const pstring_t *src);
int pstrdec_url(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes `src` as a URL compatible string.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_base32"></a>

## `pstrenc_base32`

```c
int pstrenc_base32(pstring_t *dst, const pstring_t *src);
int pstrdec_base32(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes RFC 4648 base32 (the `A-Z2-7` alphabet, `=` padding).
Unlike base64, base32's alphabet avoids visually ambiguous characters
(no `0`/`O`, `1`/`I`/`l`) and is case-insensitive on decode, which is
why it shows up in things meant to be typed by hand: TOTP/2FA secret
keys, DNSSEC/DANE record encodings, and Crockford-style IDs (though
Crockford's own variant uses a different alphabet, not this one).

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_base64"></a>

## `pstrenc_base64`

```c
int pstrenc_base64(pstring_t *dst, const pstring_t *src);
int pstrdec_base64(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes `src` as a Base64-encoded string.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_base64url"></a>

## `pstrenc_base64url`

```c
int pstrenc_base64url(pstring_t *dst, const pstring_t *src);
int pstrdec_base64url(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes `src` as a URL-safe Base64-encoded string.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_base64table"></a>

## `pstrenc_base64table`

```c
int pstrenc_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *table
);
int pstrdec_base64table(
    pstring_t *dst, const pstring_t *src, const pstring_t *table
);
```

Encodes/decodes `src` as a Base64-encoded string using the provided
translation table. `table` must be exactly 64 characters long.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_cstring"></a>

## `pstrenc_cstring`

```c
int pstrenc_cstring(pstring_t *dst, const pstring_t *src);
int pstrdec_cstring(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes an escaped version of `src` into `dst`, which, if surrounded
by quotes, is safe to use as a string literal in C source code.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_utf8"></a>

## `pstrenc_utf8`

```c
int pstrenc_utf8(pstring_t *dst, const uint32_t *src, size_t length);
int pstrdec_utf8(uint32_t *dst, size_t *length, const pstring_t *src);
```

Encodes/decodes codepoints from `src` as UTF8 characters.

For decoding variant, parameter `length` should point to the maximum length
of the buffer `dst`, which will be changed by the function to the number of
codepoints decoded.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_utf16"></a>

## `pstrenc_utf16`

```c
int pstrenc_utf16(
    pstring_t *dst, const uint32_t *src, size_t length, int endian
);
```

Encodes codepoints from `src` as UTF-16 code units into `dst`, using
`endian` byte order. Codepoints above `0xFFFF` are encoded as a
surrogate pair (four bytes); a codepoint in the surrogate range
(`0xD800`-`0xDFFF`) or above `0x10FFFF` is invalid UTF-16 and is
replaced with the replacement character `U+FFFD`, matching how
`pstrdec_utf8` already handles invalid input rather than failing.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrdec_utf16"></a>

## `pstrdec_utf16`

```c
int pstrdec_utf16(
    uint32_t *dst, size_t *length, const pstring_t *src, int endian
);
```

Decodes UTF-16 code units from `src` (byte order `endian`) into
Unicode codepoints in `dst`. `length` should point to the capacity of
`dst` in codepoints (not code units); it is updated to the number of
codepoints actually decoded. An unpaired or out-of-order surrogate
decodes to `U+FFFD` and consumes just the one code unit, so decoding
never gets permanently stuck on malformed input.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_json"></a>

## `pstrenc_json`

```c
int pstrenc_json(pstring_t *dst, const pstring_t *src);
int pstrdec_json(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes `src` as a JSON string into `dst`.

**Errors**

EINVAL, ENOMEM, EENCODE, EDECODE.

<a id="pstrenc_xml"></a>

## `pstrenc_xml`

```c
int pstrenc_xml(pstring_t *dst, const pstring_t *src);
int pstrdec_xml(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes `src` as a XML string into `dst`.

**Errors**

EINVAL, ENOMEM, ENOENT, EENCODE, EDECODE.

<a id="pstrenc_html"></a>

## `pstrenc_html`

```c
PSTR_INLINE int pstrenc_html(pstring_t *dst, const pstring_t *src);
PSTR_INLINE int pstrdec_html(pstring_t *dst, const pstring_t *src);
```

Encodes/decodes `src` as a HTML string into `dst`.

**Errors**

EINVAL, ENOMEM, ENOENT, EENCODE, EDECODE.

<a id="pstr_read_utf8"></a>

## `pstr_read_utf8`

```c
const char *pstr_read_utf8(
    const char *chr, const char *end, uint32_t *out
);
```

Reads a UTF-8 character and stores it in `out` if it's not `NULL`.
Returns a pointer to the first byte after the read character.

<a id="pstr_write_utf8"></a>

## `pstr_write_utf8`

```c
char *pstr_write_utf8(char *out, uint32_t c);
```

Writes a UTF-8 character and stores it in `out`, returning the
end of the written byte sequence. The output buffer should
be big enough to store at least 4 bytes.

<a id="pstr_read_utf16"></a>

## `pstr_read_utf16`

```c
const char *pstr_read_utf16(
    const char *chr, const char *end, uint32_t *out, int endian
);
```

Reads one UTF-16 code unit sequence (one code unit, or a surrogate
pair) starting at `chr` and stores the decoded codepoint in `out` if
it's not `NULL`. Returns a pointer to the first byte after the
sequence read, which is always `chr + 2` (a lone/invalid code unit)
or `chr + 4` (a valid surrogate pair).

<a id="pstr_write_utf16"></a>

## `pstr_write_utf16`

```c
char *pstr_write_utf16(char *out, uint32_t c, int endian);
```

Writes codepoint `c` as one or two UTF-16 code units (four bytes for
a surrogate pair), returning the end of the written sequence. The
output buffer should be big enough to store at least 4 bytes. A
codepoint that cannot be represented in UTF-16 (in the surrogate
range, or above `0x10FFFF`) is replaced with `U+FFFD`.

