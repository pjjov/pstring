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

#ifndef PSTRING_H
#define PSTRING_H

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#include <stdarg.h>
#include <stddef.h>

typedef struct allocator_t allocator_t;
struct tm; /* <time.h> */

#ifndef PSTRING_SSO_EXTEND
    #define PSTRING_SSO_EXTEND 0
#endif

#define PSTRING_SSO_SIZE (sizeof(struct pstring_base) + PSTRING_SSO_EXTEND - 2)
#define PSTRING_BLEND(x, y, mask) ((x) ^ (((x) ^ (y)) & (mask)))

typedef struct pstring_t {
    char *buffer;
    union {
        struct pstring_base {
            size_t length;
            size_t capacity;
            allocator_t *allocator;
        } base;
        struct pstring_sso {
            char buffer[PSTRING_SSO_SIZE + 1];
            unsigned char length;
        } sso;
    };
} pstring_t;

enum pstring_bool {
    PSTRING_TRUE = 1,
    PSTRING_FALSE = 0,
};

/** Error codes are negated versions of POSIX ones. **/
enum pstring_error {
    PSTRING_OK = 0,
    PSTRING_ENOENT = -2,
    PSTRING_EINTR = -4,
    PSTRING_EIO = -5,
    PSTRING_ENOMEM = -12,
    PSTRING_EEXIST = -17,
    PSTRING_EINVAL = -22,
    PSTRING_EDOM = -33,
    PSTRING_ERANGE = -34,
    PSTRING_ENOSYS = -38,
    PSTRING_ENODATA = -61,
};

/** Returns the character buffer of `str`. If `str` is resized after calling
    this function, the returned pointer should be considered invalid.
**/
PSTR_INLINE char *pstrbuf(const pstring_t *str) {
#ifndef PSTRING_SKIP_NULL_CHECKS
    if (!str)
        return NULL;
#endif

    return str->buffer ? str->buffer : (char *)str->sso.buffer;
}

/** Returns the length, number of bytes, of `str`. **/
PSTR_INLINE size_t pstrlen(const pstring_t *str) {
#ifndef PSTRING_SKIP_NULL_CHECKS
    if (!str)
        return 0;
#endif

    size_t mask = (str->buffer == NULL) - 1;
    return PSTRING_BLEND(str->sso.length, str->base.length, mask);
}

/** Returns the number of bytes allocated by `str`. **/
PSTR_INLINE size_t pstrcap(const pstring_t *str) {
#ifndef PSTRING_SKIP_NULL_CHECKS
    if (!str)
        return 0;
#endif

    size_t mask = (str->buffer == NULL) - 1;
    return PSTRING_BLEND(PSTRING_SSO_SIZE, str->base.capacity, mask);
}

/** Returns the allocator used by `str` or `NULL` if it's a slice. **/
PSTR_INLINE allocator_t *pstrallocator(const pstring_t *str) {
    return str && str->buffer ? str->base.allocator : NULL;
}

/** Checks if `str` is stored on the stack. **/
PSTR_INLINE int pstrsso(pstring_t *str) { return str && str->buffer == NULL; }

/** Checks if `str` can be resized (not a slice). **/
PSTR_INLINE int pstrowned(pstring_t *str) {
    return pstrsso(str) || pstrallocator(str) != NULL;
}

/** Returns the address pointing to the end of the character buffer,
    which, if `str` is owned, points to a `\0` character.
**/
PSTR_INLINE char *pstrend(const pstring_t *str) {
    return &pstrbuf(str)[pstrlen(str)];
}

/** Returns the character at index `i` or `'\0'` if out of bounds **/
PSTR_INLINE char pstrget(const pstring_t *str, size_t i) {
    return (i < pstrlen(str)) ? pstrbuf(str)[i] : '\0';
}

/** Returns the pointer to the slot `i` or `NULL` if out of bounds **/
PSTR_INLINE char *pstrslot(const pstring_t *str, size_t i) {
    return (i < pstrcap(str)) ? &pstrbuf(str)[i] : NULL;
}

/** Initializes `out` by copying the contents of `str`.
    If `len` is `0` and `str` is not empty, `strlen` is called.
    If `alloc` is `NULL`, the standard allocator is used.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrnew(
    pstring_t *out, const char *str, size_t len, allocator_t *alloc
);

/** Copies the contents of `str` into `out`, like `pstrnew`.
    You can use this function to turn slices into owned pstrings.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdup(
    pstring_t *out, const pstring_t *str, allocator_t *allocator
);

/** Initializes `out` and reserves `capacity` bytes.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc);

/** Frees all resources used by `str`, if it is owned. */
PSTR_API void pstrfree(pstring_t *str);

/** When `PSTRING_DETECT` is defined, this function detects the
    SIMD capabilities of the CPU at runtime. Otherwise, the function
    immediately exits, while the SIMD detection occurs at compile-time.
**/
PSTR_API void pstrdetect(void);

/** Initializes `out` as a slice, using the `buffer` for storage.
    If `length` is `0`, `strlen` is used to calculate it's length.
    If `length` is `0` and capacity is not, `strnlen` is used instead.
    If `capacity` is `0` it is set to the computed length.
    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrwrap(
    pstring_t *out, char *buffer, size_t length, size_t capacity
);

/** Initializes `out` as a slice of bytes from `str`, starting at `from`
    (inclusive) and ending at `to` (exclusive). Both indices are set to
    the length of `str` if they are larger. If `to` is smaller than
    `from`, a zero-length slice at `to` is taken.
    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrslice(
    pstring_t *out, const pstring_t *str, size_t from, size_t to
);

/** Removes characters around the specified slice of bytes from `str`,
    starting at `from` (inclusive) and ending at `to` (exclusive).
    Both indices are set to the length of `str` if they are larger.
    If `to` is smaller than `from`, the length of `str` is set to zero.

    > If `str` is a slice, it is repositioned instead.

    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrcut(pstring_t *str, size_t from, size_t to);

/** Initializes `out` as a range of bytes from `str`, starting
    at `from` (inclusive) and ending at `to` (exclusive).
    If `from` is `NULL` or invalid, it's set to the start of the pstring.
    If `to` is `NULL` or invalid, it's set to the end of the pstring.
    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrrange(
    pstring_t *out, const pstring_t *str, const char *from, const char *to
);

/** Initializes a `pstring_t` as a slice of `str`, which is assumed
    to be a null-terminated statically allocated string/array.
**/
#define PSTRWRAP(str)                                  \
    ((pstring_t) { .buffer = (str),                    \
                   .base.length = sizeof((str)) - 1,   \
                   .base.capacity = sizeof((str)) - 1, \
                   .base.allocator = 0 })

/** Returns a reference to `pstring_t` that is initialized as a slice of `str`,
    which is assumed to be a null-terminated statically allocated string/array.
**/
#define PSTR(str) (&PSTRWRAP((str)))

/** Reserves space to fit additional `count` items in `str`
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrreserve(pstring_t *str, size_t count);

/** Extends `str`'s buffer by at least `count` bytes.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrgrow(pstring_t *str, size_t count);

/** Shrinks `str`'s buffer to use as little space as possible.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrshrink(pstring_t *str);

/** Removes all characters from `str`, setting it's length to 0. **/
#define pstrclear(str) pstr__setlen((str), 0)

/** Checks if `left` and `right` pstring are equal. **/
PSTR_API int pstrequal(const pstring_t *left, const pstring_t *right);

/** Checks if `left` pstring and `right` string are equal.
    If `length` is zero, `right` is treated as a null-terminated string.
**/
PSTR_API int pstrequals(
    const pstring_t *left, const char *right, size_t length
);

/** Compares `left` and `right` lexicographically, returning:
    - a positive number if `left` should appear before `right`.
    - a negative number if `left` should appear after `right`.
    - `0` if they are equal.
**/
PSTR_API int pstrcmp(const pstring_t *left, const pstring_t *right);

/** Searches for character `ch` from the start of `str`,
    returning it's address if found and `NULL` otherwise.
**/
PSTR_API char *pstrchr(const pstring_t *str, int ch);

/** Searches for character `ch` from the end of `str`,
    returning it's address if found and `NULL` otherwise.
**/
PSTR_API char *pstrrchr(const pstring_t *str, int ch);

/** Searches for a character in `str` that is also found in `set`,
    returning it's address if found and `NULL` otherwise.
**/
PSTR_API char *pstrpbrk(const pstring_t *str, const char *set);

/** Searches for a character in `str` that is not found in `set`,
    returning it's address if found and `NULL` otherwise.
**/
PSTR_API char *pstrcpbrk(const pstring_t *str, const char *set);

/** Searches for a character in `str` that is also found in `set`,
    from behind, returning it's address if found and `NULL` otherwise.
**/
PSTR_API char *pstrrpbrk(const pstring_t *str, const char *set);

/** Searches for a character in `str` that is not found in `set`,
    from behind, returning it's address if found and `NULL` otherwise.
**/
PSTR_API char *pstrrcpbrk(const pstring_t *str, const char *set);

/** Searches for `sub` inside `str`, returning the address of the
    first character of the first match, or `NULL` if not found.
**/
PSTR_API char *pstrstr(const pstring_t *str, const pstring_t *sub);

/** Tokenizes input string `src` into a sequence of tokens separated by
    a character inside `set`. If not found, `PSTRING_ENOENT` is returned.

    Tokens and state are stored in `dst`. To initialize `dst`,
    call this function with `NULL` passed for `set`.

    The next token is started at the first character not found in `set` and
    ends in the first character that is found in `set`, or the end of `src`.

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOENT.
**/
PSTR_API int pstrtok(pstring_t *dst, const pstring_t *src, const char *set);

/** Tokenizes input string `src` into a sequence of tokens separated by
    a substring `sep`. If not found, `PSTRING_ENOENT` is returned.

    Tokens and state are stored in `dst`. To initialize `dst`,
    call this function with `NULL` passed for `sep`.

    If `sep` comes right after the end of `dst`, it is skipped before searching
    for the next token. This behaviour can be suprising when using  different
    separators between function calls.

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOENT.
**/
PSTR_API int pstrsplit(
    pstring_t *dst, const pstring_t *src, const pstring_t *sep
);

/** Null-terminated string variant of `pstrsplit`. **/
PSTR_INLINE int pstrsplits(
    pstring_t *dst, const pstring_t *src, const char *sep, size_t length
) {
    if (sep == NULL)
        return pstrsplit(dst, src, NULL);

    pstring_t tmp;
    pstrwrap(&tmp, (char *)sep, length, length);
    return pstrsplit(dst, src, &tmp);
}

/** Returns the number of consecutive characters that appear
    at the start of `str` that are included in the `set`.
**/
PSTR_API size_t pstrspn(const pstring_t *str, const char *set);

/** Returns the number of consecutive characters that appear
    at the start of `str` that are not included in the `set`.
**/
PSTR_API size_t pstrcspn(const pstring_t *str, const char *set);

/** Returns the number of consecutive characters that appear
    at the end of `str` that are included in the `set`.
**/
PSTR_API size_t pstrrspn(const pstring_t *str, const char *set);

/** Returns the number of consecutive characters that appear
    at the end of `str` that are not included in the `set`.
**/
PSTR_API size_t pstrrcspn(const pstring_t *str, const char *set);

/** Concatenates `src` onto the end of `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrcat(pstring_t *dst, const pstring_t *src);

/** Concatenates `src` onto the end of `dst`.
    If `length` is zero, `src` is treated as a null-terminated string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrcats(pstring_t *dst, const char *src, size_t length);

/** Concatenates character `chr` onto the end of `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrcatc(pstring_t *dst, char chr);

/** Concatenates `src` onto the start of `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrrcat(pstring_t *dst, const pstring_t *src);

/** Concatenates `src` onto the start of `dst`.
    If `length` is zero, `src` is treated as a null-terminated string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_INLINE int pstrrcats(pstring_t *dst, const char *src, size_t length) {
    pstring_t tmp;
    pstrwrap(&tmp, (char *)src, length, length);
    return pstrrcat(dst, &tmp);
}

/** Concatenates character `chr` onto the start of `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_INLINE int pstrrcatc(pstring_t *dst, char chr) {
    return pstrrcats(dst, &chr, 1);
}

/** Copies the contents of `src` into `dst`
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrcpy(pstring_t *dst, const pstring_t *src);

/** Concatenates `count` pstrings from `srcs` onto `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrjoin(pstring_t *dst, const pstring_t *srcs, size_t count);

/** Replaces at most `max` instances of substring `src` with `dst`.
    If `max` is zero, all instances of `src` will be replaced.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrrepl(
    pstring_t *str, const pstring_t *src, const pstring_t *dst, size_t max
);

/** Replaces at most `max` instances of substring `src` with `dst`.
    If `max` is zero, all instances of `src` will be replaced.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrrepls(
    pstring_t *str, const char *src, const char *dst, size_t max
);

/** Replaces at most `max` instances of character `src` with `dst`.
    If `max` is zero, all instances of `src` will be replaced.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrreplc(pstring_t *str, char src, char dst, size_t max);

/** Returns a non-unique integer value representing the contents of `str`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API size_t pstrhash(const pstring_t *str);

/** Concatenates the contents of the file onto the end of `out`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
PSTR_API int pstrread(pstring_t *out, const char *path);

/** Writes the entire string `out` to the file at `path`.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
PSTR_API int pstrwrite(const pstring_t *str, const char *path);

/** Concatenates a string formatted by `fmt` using date and time from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrftime(pstring_t *dst, const char *fmt, struct tm *src);

/** Concatenates a string formatted according to `fmt`. Alongside standard
    formatting options, this function offers a couple of extensions:

    - `%P` - prints the passed `pstring_t *` argument.
    - `%D` - prints calendar time using the passed format.
    - `%?` - serializes a pointer to a type indicated by the passed `int` id.
    - `%Ib`, `%Ub` - prints `int8_t` and `uint8_t` respectively.
    - `%Iw`, `%Uw` - prints `int16_t` and `uint16_t` respectively.
    - `%Id`, `%Ud` - prints `int32_t` and `uint32_t` respectively.
    - `%Iq`, `%Uq` - prints `int64_t` and `uint64_t` respectively.
    - `%Im`, `%Um` - prints `intmax_t` and `uintmax_t` respectively.
    - `%Ip`, `%Up` - prints `intptr_t` and `uintptr_t` respectively.
    - `%IP`, `%Us` - prints `ptrdiff_t` and `size_t` respectively.

    Example of extension formats:

    ```c
        pstring_t *str, *other;
        pstrfmt(str, "%P", other);

        int num;
        pstrfmt(str, "%?", PF_TYPE_INT, &num);

        struct tm date = { .tm_mday = 30 };
        pstrfmt(str, "%D", "%A %c", &date);
    ```

    > If you don't want to use extensions, use `pstrio_printf` instead.

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrfmt(pstring_t *dst, const char *fmt, ...);

/** Variable argument list variant of `pstrfmt`. **/
PSTR_API int pstrfmtv(pstring_t *dst, const char *fmt, va_list args);

/** Removes trailing characters from `str` that are specified in
    `chars`. If `str` is a slice, it will be resliced to omit them instead.

    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrrstrip(pstring_t *str, const char *chars);

/** Removes leading characters from `str` that are specified in
    `chars`. If `str` is a slice, it will be resliced to omit them instead.

    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrlstrip(pstring_t *str, const char *chars);

/** Removes leading and trailing characters from `str` that are specified in
    `chars`. If `str` is a slice, it will be resliced to omit them instead.

    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrstrip(pstring_t *str, const char *chars);

/** Checks if `str` starts with a `prefix`. **/
PSTR_API int pstrprefix(pstring_t *str, const char *prefix, size_t length);

/** Checks if `str` ends with a `suffix`. **/
PSTR_API int pstrsuffix(pstring_t *str, const char *suffix, size_t length);

/** Naively sets the length of `str` to `length` **/
PSTR_INLINE void pstr__setlen(pstring_t *str, size_t length) {
    if (pstrsso(str))
        str->sso.length = length;
    else
        str->base.length = length;

    if (pstrowned(str))
        *pstrend(str) = '\0';
}

PSTR_API size_t pstr__nlen(const char *str, size_t max);

#endif
