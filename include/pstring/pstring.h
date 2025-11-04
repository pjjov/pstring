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

#include "allocator.h"
#include <stddef.h>

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
    PSTRING_ENOSYS = -38,
    PSTRING_ENODATA = -61,
};

/** Returns the character buffer of `str`. If `str` is resized after calling
    this function, the returned pointer should be considered invalid.
**/
static inline char *pstrbuf(const pstring_t *str) {
    return str->buffer ? str->buffer : (char *)str->sso.buffer;
}

/** Returns the length, number of bytes, of `str`. **/
static inline size_t pstrlen(const pstring_t *str) {
    size_t mask = (str->buffer == NULL) - 1;
    return PSTRING_BLEND(str->sso.length, str->base.length, mask);
}

/** Returns the number of bytes allocated by `str`. **/
static inline size_t pstrcap(const pstring_t *str) {
    size_t mask = (str->buffer == NULL) - 1;
    return PSTRING_BLEND(PSTRING_SSO_SIZE, str->base.capacity, mask);
}

/** Returns the allocator used by `str` or `NULL` if it's a slice. **/
static inline allocator_t *pstrallocator(const pstring_t *str) {
    return str && str->buffer ? str->base.allocator : NULL;
}

/** Checks if `str` is stored on the stack. **/
static inline int pstrsso(pstring_t *str) { return str->buffer == NULL; }

/** Checks if `str` can be resized (not a slice). **/
static inline int pstrowned(pstring_t *str) {
    return pstrsso(str) || pstrallocator(str) != NULL;
}

/** Returns the address pointing to the end of the character buffer,
    which, if `str` is owned, points to a `\0` character.
**/
static inline char *pstrend(const pstring_t *str) {
    return &pstrbuf(str)[pstrlen(str)];
}

/** Returns the character at index `i` or `'\0'` if out of bounds **/
static inline char pstrget(const pstring_t *str, size_t i) {
    return (i < pstrlen(str)) ? pstrbuf(str)[i] : '\0';
}

/** Initializes `out` by copying the contents of `str`.
    If `len` is `0` and `str` is not empty, `strlen` is called.
    If `alloc` is `NULL`, the standard allocator is used.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrnew(pstring_t *out, const char *str, size_t len, allocator_t *alloc);

/** Copies the contents of `str` into `out`, like `pstrnew`.
    You can use this function to turn slices into owned pstrings.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrdup(pstring_t *out, const pstring_t *str, allocator_t *allocator);

/** Initializes `out` and reserves `capacity` bytes.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc);

/** Frees all resources used by `str`, if it is owned. */
void pstrfree(pstring_t *str);

/** When `PSTRING_DETECT` is defined, this function detects the
    SIMD capabilities of the CPU at runtime. Otherwise, the function
    immediately exits, while the SIMD detection occurs at compile-time.
**/
void pstrdetect(void);

/** Initializes `out` as a slice, using the `buffer` for storage.
    If `length` is `0`, `strlen` is used to calculate it's length.
    If `length` is `0` and capacity is not, `strnlen` is used instead.
    If `capacity` is `0` it is set to the computed length.
    Possible error codes: PSTRING_EINVAL.
**/
int pstrwrap(pstring_t *out, char *buffer, size_t length, size_t capacity);

/** Initializes `out` as a slice of bytes from `str`, starting at `from`
    (inclusive) and ending at `to` (exclusive). Both indices are set to
    the length of `str` if they are larger. If `to` is smaller than
    `from`, a zero-length slice at `to` is taken.
    Possible error codes: PSTRING_EINVAL.
**/
int pstrslice(pstring_t *out, const pstring_t *str, size_t from, size_t to);

/** Initializes `out` as a range of bytes from `str`, starting
    at `from` (inclusive) and ending at `to` (exclusive).
    If `from` is `NULL` or invalid, it's set to the start of the pstring.
    If `to` is `NULL` or invalid, it's set to the end of the pstring.
    Possible error codes: PSTRING_EINVAL.
**/
int pstrrange(
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

/** Reserves space to fit additional `count` items in `str`
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrreserve(pstring_t *str, size_t count);

/** Extends `str`'s buffer by at least `count` bytes.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrgrow(pstring_t *str, size_t count);

/** Shrinks `str`'s buffer to use as little space as possible.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrshrink(pstring_t *str);

/** Removes all characters from `str`, setting it's length to 0. **/
#define pstrclear(str) pstr__setlen((str), 0)

/** Checks if `left` and `right` pstring are equal. **/
int pstrequal(const pstring_t *left, const pstring_t *right);

/** Compares `left` and `right` lexicographically, returning:
    - a positive number if `left` should appear before `right`.
    - a negative number if `left` should appear after `right`.
    - `0` if they are equal.
**/
int pstrcmp(const pstring_t *left, const pstring_t *right);

/** Searches for character `ch` from the start of `str`,
    returning it's address if found and `NULL` otherwise.
**/
char *pstrchr(const pstring_t *str, int ch);

/** Searches for character `ch` from the end of `str`,
    returning it's address if found and `NULL` otherwise.
**/
char *pstrrchr(const pstring_t *str, int ch);

/** Searches for a character in `str` that is also found in `set`,
    returning it's address if found and `NULL` otherwise.
**/
char *pstrpbrk(const pstring_t *str, const char *set);

/** Searches for a character in `str` that is not found in `set`,
    returning it's address if found and `NULL` otherwise.
**/
char *pstrcpbrk(const pstring_t *str, const char *set);

/** Searches for `sub` inside `str`, returning the address of the
    first character of the first match, or `NULL` if not found.
**/
char *pstrstr(const pstring_t *str, const pstring_t *sub);

/** Returns the number of consecutive characters that appear
    at the start of `str` that are included in the `set`.
**/
size_t pstrspn(const pstring_t *str, const char *set);

/** Returns the number of consecutive characters that appear
    at the start of `str` that are not included in the `set`.
**/
size_t pstrcspn(const pstring_t *str, const char *set);

/** Returns the number of consecutive characters that appear
    at the end of `str` that are included in the `set`.
**/
size_t pstrrspn(const pstring_t *str, const char *set);

/** Returns the number of consecutive characters that appear
    at the end of `str` that are not included in the `set`.
**/
size_t pstrrcspn(const pstring_t *str, const char *set);

/** Concatenates `src` onto the end of `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrcat(pstring_t *dst, const pstring_t *src);

/** Copies the contents of `src` into `dst`
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrcpy(pstring_t *dst, const pstring_t *src);

/** Concatenates `count` pstrings from `srcs` onto `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrjoin(pstring_t *dst, const pstring_t *srcs, size_t count);

/** Replaces at most `max` instances of substring `src` with `dst`.
    If `max` is zero, all instances of `src` will be replaced.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
int pstrrepl(
    pstring_t *str, const pstring_t *src, const pstring_t *dst, size_t max
);

/** Concatenates the contents of the file onto the end of `out`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
int pstrread(pstring_t *out, const char *path);

/** Writes the entire string `out` to the file at `path`.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
int pstrwrite(const pstring_t *str, const char *path);

/** Naively sets the length of `str` to `length` **/
static inline void pstr__setlen(pstring_t *str, size_t length) {
    if (pstrsso(str))
        str->sso.length = length;
    else
        str->base.length = length;
}

size_t pstr__nlen(const char *str, size_t max);

#endif
