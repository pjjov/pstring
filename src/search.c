/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/core.h>
#include <pstring/search.h>

#include <string.h>

/* SIMD accelerated functions are in 'simd.c'. */

char *pstrstr(const pstring_t *str, const pstring_t *sub) {
    if (!str || !sub)
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    if (pstrlen(sub) > pstrlen(str))
        return NULL;

    if (pstrlen(sub) == 0)
        return pstrbuf(str);

    char ch = pstrbuf(sub)[0];
    char *end = pstrend(str) - pstrlen(sub) + 1;

    char *match;
    pstring_t search;
    pstrrange(&search, NULL, pstrbuf(str), end);

    while ((match = pstrchr(&search, ch))) {
        if (0 == memcmp(match, pstrbuf(sub), pstrlen(sub)))
            return match;
        pstrrange(&search, NULL, match + 1, end);
    }

    return NULL;
}

int pstrtok(pstring_t *dst, const pstring_t *src, const char *set) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (!set) {
        pstrslice(dst, src, 0, 0);
        return PSTRING_OK;
    }

    pstring_t search;

    pstrrange(&search, src, pstrend(dst), pstrend(src));
    const char *start = pstrcpbrk(&search, set);

    pstrrange(&search, src, start, pstrend(src));
    const char *end = pstrpbrk(&search, set);

    if (!start)
        return PSTRING_ENOENT;

    pstrrange(dst, src, start, end ? end : pstrend(src));
    return PSTRING_OK;
}

int pstrsplit(pstring_t *dst, const pstring_t *src, const pstring_t *sep) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (!sep) {
        pstrslice(dst, src, 0, 0);
        return PSTRING_OK;
    }

    pstring_t search;
    const char *prev = pstrend(dst);

    pstrrange(&search, src, prev, pstrend(src));
    if (pstrprefix(&search, pstrbuf(sep), pstrlen(sep))) {
        prev += pstrlen(sep);
        pstrrange(&search, src, prev, pstrend(src));
    }

    const char *next = pstrstr(&search, sep);
    pstrrange(dst, src, prev, next ? next : pstrend(src));
    return prev == pstrend(src) ? PSTRING_ENOENT : PSTRING_OK;
}

int pstrsplits(
    pstring_t *dst, const pstring_t *src, const char *sep, size_t length
) {
    if (sep == NULL)
        return pstrsplit(dst, src, NULL);
    pstring_t tmp;
    pstrwrap(&tmp, (char *)sep, length, length);
    return pstrsplit(dst, src, &tmp);
}

int pstrprefix(const pstring_t *str, const char *prefix, size_t length) {
    if (!str || !prefix)
        return PSTRTHROW_EINVAL;

    if (length == 0)
        length = strlen(prefix);

    if (length > pstrlen(str))
        return PSTRING_FALSE;

    return 0 == memcmp(pstrbuf(str), prefix, length);
}

int pstrsuffix(const pstring_t *str, const char *suffix, size_t length) {
    if (!str || !suffix)
        return PSTRTHROW_EINVAL;

    if (length == 0)
        length = strlen(suffix);

    if (length > pstrlen(str))
        return PSTRING_FALSE;

    return 0 == memcmp(pstrslot(str, pstrlen(str) - length), suffix, length);
}