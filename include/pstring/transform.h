/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_TRANSFORM_H
#define PSTRING_TRANSFORM_H

/** Module: String transformation functions

    This module provides function for inserting, removing and replacing
    parts of the string.
*/

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct pstring_t pstring_t;

/** Concatenates `src` onto the end of `dst`.
    Errors: EINVAL, ENOMEM.
*/
PSTR_API int pstrcat(pstring_t *dst, const pstring_t *src);
PSTR_API int pstrcats(pstring_t *dst, const char *src, size_t length);
PSTR_API int pstrcatc(pstring_t *dst, char chr);
PSTR_INLINE int pstrcatb(pstring_t *dst, const char *src, size_t length) {
    return length > 0 ? pstrcats(dst, src, length) : 0;
}

/** Concatenates `src` onto the start of `dst`.
    Errors: EINVAL, ENOMEM.
*/
PSTR_API int pstrrcat(pstring_t *dst, const pstring_t *src);
PSTR_API int pstrrcats(pstring_t *dst, const char *src, size_t length);
PSTR_INLINE int pstrrcatc(pstring_t *dst, char chr) {
    return pstrrcats(dst, &chr, 1);
}

/** Inserts characters from `src` into `dst` at index `at`.
    Errors: EINVAL, ENOMEM.
*/
PSTR_API int pstrinsert(pstring_t *dst, size_t at, pstring_t *src);
PSTR_API int pstrinserts(
    pstring_t *dst, size_t at, const char *src, size_t length
);

/** Inserts `chr` character `count` times into `dst` at index `at`.
    Errors: EINVAL, ENOMEM.
*/
PSTR_API int pstrinsertc(pstring_t *dst, size_t at, size_t count, char chr);

/** Removes characters from `str` in the specified range.
    Errors: EINVAL, ENOMEM.
*/
PSTR_API int pstrremove(pstring_t *str, size_t from, size_t to);

/** Concatenates `count` pstrings from `srcs` onto `dst`.
    Errors: EINVAL, ENOMEM.
*/
PSTR_API int pstrjoin(pstring_t *dst, const pstring_t *srcs, size_t count);

/** Replaces at most `max` instances of substring `src` with `dst`.
    If `max` is zero, all instances of `src` will be replaced.
    Errors: EINVAL, ENOMEM.
*/
PSTR_API int pstrrepl(
    pstring_t *str, const pstring_t *src, const pstring_t *dst, size_t max
);
PSTR_API int pstrrepls(
    pstring_t *str, const char *src, const char *dst, size_t max
);
PSTR_API int pstrreplc(pstring_t *str, char src, char dst, size_t max);

/** Removes leading and trailing characters from `str` that are specified in
    `chars`. If `str` is a slice, it will be resliced to omit them instead.

    The `pstrlstrip` variant only removes leading, while `pstrrstrip` only
    removes trailing characters from `str` that are specified in `chars`.

    Errors: EINVAL.
*/
PSTR_API int pstrstrip(pstring_t *str, const char *chars);
PSTR_API int pstrlstrip(pstring_t *str, const char *chars);
PSTR_API int pstrrstrip(pstring_t *str, const char *chars);

/** Removes leading whitespace up to `count`, assuming that `\t` character
    is equivalent to `tab` blank characters (defaults to 4 instead).
    If `count` is zero or less, all whitespace is removed.

    Errors: EINVAL.
*/
PSTR_API int pstrdedent(pstring_t *str, int count, int tab);

/** Inserts leading whitespace up to `count`, assuming that `\t` character
    is equivalent to `tab` blank characters (defaults to 4 instead). If `count`
    is zero or less, the minimum indentation already present is returned.

    Errors: EINVAL.
*/
PSTR_API int pstrindent(pstring_t *str, int count, int tab);

#ifdef __cplusplus
}
#endif

#endif /* PSTRING_TRANSFORM_H */