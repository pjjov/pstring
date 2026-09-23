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

#ifndef PSTRING_CORE_H
#define PSTRING_CORE_H

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#ifndef PSTR_NO_RETURN
    #define PSTR_NO_RETURN
#endif

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct allocator_t allocator_t;
typedef struct pf_exception_t pf_exception_t;

/** ## NAME

    **pstring** - fully-featured string library for C

    ## DESCRIPTION

    The `pstring_t` structure describes strings using 4 variables:
    - string's character buffer (`pstrbuf`),
    - the number of characters inside the buffer (`pstrlen`),
    - buffer's size/capacity (`pstrcap`),
    - allocator which owns the buffer's memory (`pstrallocator`).

    Strings initialized with `{0}` or created using `pstrnew`, `pstralloc`,
    `pstrdup` and others, own their buffers and can be resized and expanded,
    but require calling `pstrfree` to free memory resources.

    Strings can also be initialized as slices using `pstrwrap`, `pstrslice` and
    `pstrrange`. Since slices represent chunks of other strings, they don't
    need to be freed, but their buffer needs to be valid during their usage.

    Shorter strings can be stored directly in the `pstring_t` using SSO,
    provided they have been allocated using the default allocator. This mechanism
    is not noticable most of the time, but can cause obscure bugs if misused.

    [TOC]

    ## REFERENCE
**/

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

typedef struct pstrarray_t {
    size_t capacity;
    size_t length;
    pstring_t *items;
    allocator_t *allocator;
} pstrarray_t;

enum pstring_bool {
    PSTRING_TRUE = 1,
    PSTRING_FALSE = 0,
};

#define PSTRING_EXCEPTION (('P' << 8) | 'S')

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
    PSTRING_EENCODE = -192,
    PSTRING_EDECODE = -193,
    PSTRING_EOBJECT = -194,
};

/** This function set's up the exception handler for the `<pf_exception.h>`
    exception handling interface. The first call will return `NULL`, after
    which exception-throwing code can be executed.

    The second return value will represent the emitted exception.

    > The global exception stack is thread local.
**/
PSTR_API pf_exception_t *pstrcatch(int error);

#define PSTRTHROW(code, msg) pstrthrow((code), __func__, (msg))
#define PSTRTHROWF(code, fmt, ...)                   \
    pstrthrowf((code), __func__, (fmt), __VA_ARGS__)
#define PSTRVTHROWF(code, fmt, args)             \
    pstrvthrowf((code), __func__, (fmt), (args))

#define PSTRTHROW_NULL(code) (PSTRTHROW(code, NULL), NULL)

#define PSTRTHROW_ENOENT PSTRTHROW(PSTRING_ENOENT, NULL)
#define PSTRTHROW_EINTR PSTRTHROW(PSTRING_EINTR, NULL)
#define PSTRTHROW_EIO PSTRTHROW(PSTRING_EIO, NULL)
#define PSTRTHROW_ENOMEM PSTRTHROW(PSTRING_ENOMEM, NULL)
#define PSTRTHROW_EEXIST PSTRTHROW(PSTRING_EEXIST, NULL)
#define PSTRTHROW_EINVAL PSTRTHROW(PSTRING_EINVAL, NULL)
#define PSTRTHROW_EDOM PSTRTHROW(PSTRING_EDOM, NULL)
#define PSTRTHROW_ERANGE PSTRTHROW(PSTRING_ERANGE, NULL)
#define PSTRTHROW_ENOSYS PSTRTHROW(PSTRING_ENOSYS, NULL)
#define PSTRTHROW_ENODATA PSTRTHROW(PSTRING_ENODATA, NULL)
#define PSTRTHROW_EENCODE PSTRTHROW(PSTRING_EENCODE, NULL)
#define PSTRTHROW_EDECODE PSTRTHROW(PSTRING_EDECODE, NULL)
#define PSTRTHROW_EOBJECT PSTRTHROW(PSTRING_EOBJECT, NULL)

/** Emits an exception with given message. **/
PSTR_NO_RETURN PSTR_API int pstrthrow(
    int code, const char *func, const char *msg
);

/** Emits an exception with given formatting. **/
PSTR_NO_RETURN PSTR_API int pstrthrowf(
    int code, const char *func, const char *fmt, ...
);

/** Emits an exception with given formatting. **/
PSTR_NO_RETURN PSTR_API int pstrvthrowf(
    int code, const char *func, const char *fmt, va_list args
);

/** Emits the exception `e` for the next catcher. **/
PSTR_NO_RETURN PSTR_API int pstrrethrow(pf_exception_t *e);

/** When `PSTRING_DETECT` is defined, this function detects the
    SIMD capabilities of the CPU at runtime. Otherwise, the function
    immediately exits, while the SIMD detection occurs at compile-time.
**/
PSTR_API void pstrdetect(void);

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
PSTR_INLINE int pstrsso(const pstring_t *str) {
    return str && str->buffer == NULL;
}

/** Checks if `str` can be resized (not a slice). **/
PSTR_INLINE int pstrowned(const pstring_t *str) {
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

/** Frees all strings in array `a`, if they are owned. */
PSTR_API void pstrarray_free(pstrarray_t *array);

/** Copies the contents of `str` to `buffer` as a null-terminated string.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdump(const pstring_t *str, char *buffer, size_t size);

/** Ensures that `str` is null-terminated by copying it's content to a new
    buffer and appending '\0', if it's not currently null-terminated.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrterm(pstring_t *str, allocator_t *allocator);

/** Ensures that `str` is null-terminated by copying it's content to the given
    buffer and appending '\0', if it's not currently null-terminated.
    Returned string is either `buffer` or `pstrbuf(str)`.
**/
PSTR_API char *pstrterms(pstring_t *str, char *buffer, size_t size);

/** Checks if `str` is null-terminated. **/
PSTR_INLINE int pstristerm(pstring_t *str) { return pstrowned(str); }

/** Returns the character buffer of `str`. If `str` is using SSO,
    the contents of `str` will be copied to a brand new buffer.
**/
PSTR_API char *pstrunwrap(const pstring_t *str, allocator_t *allocator);

/** Initializes `out` as a slice, using the `buffer` for storage.
    If `length` is `0`, `strlen` is used to calculate it's length.
    If `length` is `0` and capacity is not, `strnlen` is used instead.
    If `capacity` is `0` it is set to the computed length.
    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrwrap(
    pstring_t *out, char *buffer, size_t length, size_t capacity
);

/** Initializes `out` as a slice, using the `buffer` for storage.
    No `strlen` is performed by this variant.
    Possible error codes: PSTRING_EINVAL.
**/
PSTR_API int pstrwrapb(
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
PSTR_API int pstrequalb(
    const pstring_t *left, const char *right, size_t length
);

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
PSTR_API int pstrcmps(const pstring_t *left, const char *right, size_t length);

/** Copies the contents of `src` into `dst`
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrcpy(pstring_t *dst, const pstring_t *src);

/** Returns a non-unique integer value representing the contents of `str`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API size_t pstrhash(const pstring_t *str);

/** Returns Damerau–Levenshtein distance between `left` and `right`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdistance(const pstring_t *left, const pstring_t *right);

/** This function slices `str` by moving the starting index by `right` and
    the ending index by `left` number of characters. (`str` must be a slice)

    Possible error codes: PSTRING_EINVAL.
**/
PSTR_INLINE int pstrshift(pstring_t *str, size_t left, size_t right) {
    if (pstrowned(str))
        return PSTRING_EINVAL;
    if (left > pstrlen(str))
        left = pstrlen(str);
    return pstrslice(str, str, right, pstrlen(str) - left);
}
PSTR_INLINE int pstrlshift(pstring_t *str, size_t count) {
    return pstrshift(str, count, 0);
}
PSTR_INLINE int pstrrshift(pstring_t *str, size_t count) {
    return pstrshift(str, 0, count);
}

/** Checks if `str` starts with a `prefix`. **/
PSTR_API int pstrprefix(
    const pstring_t *str, const char *prefix, size_t length
);

/** Checks if `str` ends with a `suffix`. **/
PSTR_API int pstrsuffix(
    const pstring_t *str, const char *suffix, size_t length
);

/** Gets the value of the environment variable `name` and slices it as `out`.
    Object `name` can be a slice, i.e. null termination is not required.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOENT, PSTRING_ENOMEM.
**/
PSTR_API int pstrenv(pstring_t *out, const pstring_t *name);

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

#ifdef __cplusplus
}
#endif

#endif /* PSTRING_CORE_H */
