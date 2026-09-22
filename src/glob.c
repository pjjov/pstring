/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/glob.h>
#include <pstring/pstring.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allocator.h>
#include <allocator_std.h>

#include <pf_dirent.h>
#include <pf_filesystem.h>

#define PF_ARRAY_USE_ALLOCATOR_T
#define PF_ARRAY_DEFAULT_ALLOCATOR NULL
#include <pf_array.h>

/** ### pstrglob_match

    A small backtracking matcher. `*` is handled by trying the shortest
    possible expansion first and only backtracking (advancing the amount
    of text `*` consumes) when the remainder of the pattern fails, which
    keeps the common case (few wildcards) close to linear instead of
    paying for exponential blowup on adversarial input. **/
static int glob_match_here(
    const char *p, const char *pend, const char *s, const char *send, int flags
) {
    while (p < pend) {
        if (*p == '*') {
            /* collapse consecutive '*' -- avoids redundant backtracking */
            while (p < pend && *p == '*')
                p++;

            if (p == pend)
                return PSTRING_TRUE; /* trailing '*' matches the rest */

            for (const char *cursor = s; cursor <= send; cursor++) {
                if (glob_match_here(p, pend, cursor, send, flags))
                    return PSTRING_TRUE;
            }
            return PSTRING_FALSE;
        }

        if (s == send)
            return PSTRING_FALSE;

        if (*p == '?') {
            p++;
            s++;
            continue;
        }

        if (*p == '[' && p + 1 < pend) {
            const char *set = p + 1;
            int negate = 0;

            if (set < pend && (*set == '!' || *set == '^')) {
                negate = 1;
                set++;
            }

            const char *setStart = set;
            const char *close = set;

            /* a ']' as the very first set character is literal */
            if (close < pend && *close == ']')
                close++;
            while (close < pend && *close != ']')
                close++;

            if (close >= pend) {
                /* unterminated '[' -- treat as a literal bracket */
                if (*s != '[')
                    return PSTRING_FALSE;
                p++;
                s++;
                continue;
            }

            int found = 0;
            for (const char *c = setStart; c < close; c++) {
                if (c + 2 < close && c[1] == '-') {
                    unsigned char lo = (unsigned char)c[0];
                    unsigned char hi = (unsigned char)c[2];
                    unsigned char ch = (unsigned char)*s;
                    if (lo <= hi ? (ch >= lo && ch <= hi)
                                 : (ch >= hi && ch <= lo)) {
                        found = 1;
                    }
                    c += 2;
                } else if (*c == *s) {
                    found = 1;
                }
            }

            if (found == negate)
                return PSTRING_FALSE;

            p = close + 1;
            s++;
            continue;
        }

        if (*p == '\\' && !(flags & PSTRGLOB_NOESCAPE) && p + 1 < pend) {
            p++;
        }

        if (*p != *s)
            return PSTRING_FALSE;

        p++;
        s++;
    }

    return s == send ? PSTRING_TRUE : PSTRING_FALSE;
}

int pstrglob_match(const char *pattern, const pstring_t *name, int flags) {
    if (!pattern || !name)
        return PSTRTHROW_EINVAL;

    const char *p = pattern;
    const char *pend = pattern + strlen(pattern);
    const char *s = pstrbuf(name);
    const char *send = pstrend(name);

    if (!(flags & PSTRGLOB_PERIOD) && s < send && *s == '.') {
        /* a bare leading '*'/'?' in the pattern must not match a dotfile,
           matching the historical shell convention; an explicit leading
           '.' in the pattern is unaffected since it matches literally */
        if (p < pend && (*p == '*' || *p == '?'))
            return PSTRING_FALSE;
    }

    return glob_match_here(p, pend, s, send, flags);
}

/* ---- filesystem walking ------------------------------------------ */

typedef PF_ARRAY(pstring_t) pstrglob_words_t;

static int is_separator(char c) {
#ifdef PSTRGLOB_WINDOWS
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

/** Whether joining `prefix` with another path component needs an
    inserted separator. Root prefixes ("/" on POSIX, "C:\\" on Windows)
    already end in a separator, so joining "/" + "/" + "tmp" (as a naive
    `prefix[0] ? "/" : ""` check would, since `prefix[0]` is true for a
    root of just "/") produced a "//tmp" that happened to still resolve
    correctly on POSIX but is still wrong output, and is not guaranteed
    to resolve on every platform. **/
static int needs_separator(const char *prefix) {
    size_t len = strlen(prefix);
    return len > 0 && !is_separator(prefix[len - 1]);
}

static int has_wildcard(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (s[i] == '*' || s[i] == '?' || s[i] == '[')
            return 1;
    return 0;
}

/** Lists the immediate children of `dir` (an allocator-owned,
    null-terminated path; "" means the current directory) whose name
    matches `pattern`, appending `dir/name` (or just `name` for an empty
    `dir`) to `out`. **/
static int glob_list_dir(
    const char *dir,
    const char *pattern,
    int flags,
    allocator_t *alloc,
    pstrglob_words_t *out
) {
    struct dirent *entry;
    int matched = 0;
    DIR *dp;

    if (!(dp = opendir(dir[0] ? dir : ".")))
        return PSTRING_OK;

    while ((entry = readdir(dp))) {
        if (0 == strcmp(entry->d_name, ".") || 0 == strcmp(entry->d_name, ".."))
            continue;

        pstring_t name;
        pstrwrap(&name, entry->d_name, 0, 0);

        if (!pstrglob_match(pattern, &name, flags))
            continue;

        char full[4096];
        int n = snprintf(
            full,
            sizeof(full),
            "%s%s%s",
            dir,
            needs_separator(dir) ? "/" : "",
            entry->d_name
        );
        if (n < 0 || (size_t)n >= sizeof(full)) {
            closedir(dp);
            return PSTRTHROW(PSTRING_ENOMEM, NULL);
        }

        if ((flags & PSTRGLOB_MARK) && pf_isdir(full)
            && n + 1 < (int)sizeof(full)) {
            full[n++] = '/';
            full[n] = '\0';
        }

        pstring_t item;
        if (pstrnew(&item, full, 0, alloc)) {
            closedir(dp);
            return PSTRTHROW(PSTRING_ENOMEM, NULL);
        }
        if (PF_ARRAY_PUSH(out, &item, 1)) {
            pstrfree(&item);
            closedir(dp);
            return PSTRTHROW(PSTRING_ENOMEM, NULL);
        }
        matched = 1;
    }

    closedir(dp);
    (void)matched;
    return PSTRING_OK;
}

static int glob_cmp(const void *a, const void *b) {
    return pstrcmp((const pstring_t *)a, (const pstring_t *)b);
}

/** Recursively expands one `/`-separated component of the pattern at a
    time. `prefix` accumulates the literal directories already resolved
    ("src" while matching the "*" component in a pattern like
    "src" + "*" + ".c"). Non-wildcard
    components are consumed directly without touching the filesystem
    unless they're the final component -- this both avoids unnecessary
    `readdir` calls and lets a pattern with no wildcards at all fall
    through to a plain existence check. **/
static int glob_walk(
    const char *prefix,
    const char *pat,
    size_t patlen,
    int flags,
    allocator_t *alloc,
    pstrglob_words_t *out
) {
    size_t i = 0;
    while (i < patlen && !is_separator(pat[i]))
        i++;

    char component[1024];
    if (i >= sizeof(component))
        return PSTRTHROW(PSTRING_ENOMEM, NULL);
    memcpy(component, pat, i);
    component[i] = '\0';

    int isLast = (i == patlen);
    const char *rest = pat + i + (isLast ? 0 : 1);
    size_t restlen = isLast ? 0 : patlen - i - 1;

    if (!has_wildcard(component, i)) {
        char joined[4096];
        int n = snprintf(
            joined,
            sizeof(joined),
            "%s%s%s",
            prefix,
            needs_separator(prefix) ? "/" : "",
            component
        );
        if (n < 0 || (size_t)n >= sizeof(joined))
            return PSTRTHROW(PSTRING_ENOMEM, NULL);

        if (isLast) {
            if (!pf_exists(joined))
                return PSTRING_OK;

            if ((flags & PSTRGLOB_MARK) && pf_isdir(joined)
                && n + 1 < (int)sizeof(joined)) {
                joined[n++] = '/';
                joined[n] = '\0';
            }

            pstring_t item;
            if (pstrnew(&item, joined, 0, alloc))
                return PSTRTHROW(PSTRING_ENOMEM, NULL);
            if (PF_ARRAY_PUSH(out, &item, 1)) {
                pstrfree(&item);
                return PSTRTHROW(PSTRING_ENOMEM, NULL);
            }
            return PSTRING_OK;
        }

        return glob_walk(joined, rest, restlen, flags, alloc, out);
    }

    pstrglob_words_t matches;
    PF_ARRAY_WITH_ALLOCATOR(&matches, alloc);

    int rc = glob_list_dir(prefix, component, flags, alloc, &matches);
    if (rc) {
        PF_ARRAY_FREE(&matches);
        return rc;
    }

    if (!(flags & PSTRGLOB_NOSORT) && PF_ARRAY_LEN(&matches) > 1) {
        qsort(
            PF_ARRAY_GET(&matches, 0),
            PF_ARRAY_LEN(&matches),
            sizeof(pstring_t),
            glob_cmp
        );
    }

    for (size_t k = 0; k < PF_ARRAY_LEN(&matches); k++) {
        pstring_t *dir = PF_ARRAY_SLOT(&matches, k);

        if (isLast) {
            if (PF_ARRAY_PUSH(out, dir, 1)) {
                for (; k < PF_ARRAY_LEN(&matches); k++)
                    pstrfree(PF_ARRAY_SLOT(&matches, k));
                PF_ARRAY_FREE(&matches);
                return PSTRTHROW(PSTRING_ENOMEM, NULL);
            }
            continue;
        }

        char dirbuf[4096];
        if (pstrlen(dir) >= sizeof(dirbuf)) {
            pstrfree(dir);
            continue;
        }
        memcpy(dirbuf, pstrbuf(dir), pstrlen(dir));
        dirbuf[pstrlen(dir)] = '\0';
        pstrfree(dir);

        if ((rc = glob_walk(dirbuf, rest, restlen, flags, alloc, out))) {
            for (k++; k < PF_ARRAY_LEN(&matches); k++)
                pstrfree(PF_ARRAY_SLOT(&matches, k));
            PF_ARRAY_FREE(&matches);
            return rc;
        }
    }

    PF_ARRAY_FREE(&matches);
    return PSTRING_OK;
}

int pstrglob(
    pstrarray_t *dst, const pstring_t *pattern, int flags, allocator_t *alloc
) {
    if (!dst || !pattern)
        return PSTRTHROW_EINVAL;

    if (!alloc)
        alloc = &standard_allocator;

    char pat[4096];
    /* `pstrterms` returns either the original buffer (already
       terminated) or `pat` (freshly copied) -- it must not be assumed
       to always populate `pat` itself, or an already-terminated
       pattern silently glob-matches against garbage stack contents. */
    const char *patstr = pstrterms((pstring_t *)pattern, pat, sizeof(pat));
    if (!patstr)
        return PSTRTHROW_ENOMEM;

    pstrglob_words_t words;
    PF_ARRAY_WITH_ALLOCATOR(&words, alloc);

    /* an absolute path's leading '/' is not itself a pattern component */
    const char *start = patstr;
    char root[2] = { 0, 0 };
    if (is_separator(patstr[0])) {
        root[0] = patstr[0];
        start = patstr + 1;
    }

    int rc = glob_walk(root, start, strlen(start), flags, alloc, &words);

    if (!rc && PF_ARRAY_LEN(&words) == 0) {
        if (flags & PSTRGLOB_NOCHECK) {
            pstring_t item;
            if (pstrdup(&item, pattern, alloc)
                || PF_ARRAY_PUSH(&words, &item, 1))
                rc = PSTRTHROW(PSTRING_ENOMEM, NULL);
        } else {
            rc = PSTRING_ENOENT;
        }
    }

    if (rc) {
        for (size_t i = 0; i < PF_ARRAY_LEN(&words); i++)
            pstrfree(PF_ARRAY_SLOT(&words, i));
        PF_ARRAY_FREE(&words);
        return rc;
    }

    size_t added = PF_ARRAY_LEN(&words);
    pstring_t *slots = PF_ARRAY_GET(&words, 0);

    /* An empty, freshly-zeroed `dst` can just take ownership of the
       pf_array's storage directly. Otherwise (the caller is accumulating
       results across several `pstrglob` calls, as `pstrexpand_with`'s
       pathname-expansion step does) copy element-by-element and free the
       now-empty pf_array storage. */
    if (dst->items == NULL && dst->length == 0 && dst->capacity == 0) {
        dst->items = slots;
        dst->length = added;
        dst->capacity = PF_ARRAY_CAP(&words);
        dst->allocator = alloc;
        return PSTRING_OK;
    }

    for (size_t i = 0; i < added; i++) {
        if (dst->length >= dst->capacity) {
            size_t newCap = dst->capacity ? dst->capacity * 2 : 4;
            pstring_t *grown = reallocate(
                alloc,
                dst->items,
                dst->capacity * sizeof(pstring_t),
                newCap * sizeof(pstring_t)
            );
            if (!grown) {
                for (; i < added; i++)
                    pstrfree(&slots[i]);
                deallocate(
                    alloc, slots, PF_ARRAY_CAP(&words) * sizeof(pstring_t)
                );
                return PSTRTHROW_ENOMEM;
            }
            dst->items = grown;
            dst->capacity = newCap;
        }

        dst->items[dst->length++] = slots[i];
    }

    deallocate(alloc, slots, PF_ARRAY_CAP(&words) * sizeof(pstring_t));
    return PSTRING_OK;
}
