/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_SEARCH_H
#define PSTRING_SEARCH_H

#ifndef PSTR_API
    #define PSTR_API
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct pstring_t pstring_t;

/** Searches for character `ch` from the start of `str`,
    returning it's address if found and `NULL` otherwise.

    The `pstrrchr` variant searches from the end instead.
**/
PSTR_API char *pstrchr(const pstring_t *str, int ch);
PSTR_API char *pstrrchr(const pstring_t *str, int ch);

/** Searches for a character in `str` that is also found in `set`,
    returning it's address if found and `NULL` otherwise.

    The `pstrrpbrk` and `pstrrcpbrk` variants search from the end instead.

    `pstrcpbrk` and `pstrrcpbrk` variants search for a character that is not
    found in `set` returning it's address if found and `NULL` otherwise.
**/
PSTR_API char *pstrpbrk(const pstring_t *str, const char *set);
PSTR_API char *pstrcpbrk(const pstring_t *str, const char *set);
PSTR_API char *pstrrpbrk(const pstring_t *str, const char *set);
PSTR_API char *pstrrcpbrk(const pstring_t *str, const char *set);

/** Returns the number of consecutive characters that appear
    at the start of `str` that are included in the `set`.

    The `pstrrspn` and `pstrrcspn` variants count from the end instead.

    `pstrcspn` and `pstrrcspn` variants count the number of consecutive
    characters that aren't included in `set`, from start and end respectively.
**/
PSTR_API size_t pstrspn(const pstring_t *str, const char *set);
PSTR_API size_t pstrcspn(const pstring_t *str, const char *set);
PSTR_API size_t pstrrspn(const pstring_t *str, const char *set);
PSTR_API size_t pstrrcspn(const pstring_t *str, const char *set);

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
PSTR_API int pstrsplits(
    pstring_t *dst, const pstring_t *src, const char *sep, size_t length
);

#ifdef __cplusplus
}
#endif

#endif /* PSTRING_SEARCH_H */