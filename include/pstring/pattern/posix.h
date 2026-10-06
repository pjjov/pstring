/*  pstring - fully-featured string library for C

    Copyright 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_PATTERN_POSIX_H
#define PSTRING_PATTERN_POSIX_H

/** Adapter: POSIX `<regex.h>` (extended regular expressions) engine for
    `<pstring/pattern.h>`. It needs no third-party library on POSIX systems.

        #include <pstring/pattern/posix.h>

        pstrexpr_register_posix(NULL);   // "posix/.../", and "re/.../"

    Flags:
        i  case-insensitive
        n  newline-sensitive (`.` and `[^x]` don't match newline,
           `^` and `$` match at lines)
        b  basic regular expressions (the default is extended)

    The search uses `REG_STARTEND` when the C library provides it. Without it
    the subject is copied before each search and is cut at the first NUL.
    Look-behinds, `\b` and friends cannot see the text before the start offset
    of a continued search (`iter`, `split` and `repl`) on such systems.

    > Don't combine this adapter with Oniguruma (`onig.h`): libonig exports its
    > own `regcomp` and `regexec`, which then replace the C library's.

    Back-references and named groups are not part of POSIX, so replacements
    cannot use `${name}`.
*/

#include <pstring/core.h>
#include <pstring/pattern.h>

#include <allocator.h>

#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define PSTREXPR_POSIX_STACK 32

PSTR_INLINE int pstrexpr_posix_compile(
    void **prog,
    struct pstrexpr_info *info,
    const pstring_t *pattern,
    const pstring_t *flags,
    allocator_t *alloc,
    void *user
) {
    (void)user;

    int cflags = REG_EXTENDED;
    for (size_t i = 0; i < pstrlen(flags); i++) {
        switch (pstrget(flags, i)) {
        case 'i':
            cflags |= REG_ICASE;
            break;
        case 'n':
            cflags |= REG_NEWLINE;
            break;
        case 'b':
            cflags &= ~REG_EXTENDED;
            break;
        default:
            return PSTRTHROW(PSTRING_EINVAL, "posix: unknown flag");
        }
    }

    /* regcomp wants a null-terminated string */
    size_t length = pstrlen(pattern);
    char *source = allocate(alloc, length + 1);
    regex_t *re = zallocate(alloc, sizeof(*re));
    if (!source || !re) {
        deallocate(alloc, source, length + 1);
        deallocate(alloc, re, sizeof(*re));
        return PSTRTHROW_ENOMEM;
    }

    pstrdump(pattern, source, length + 1);
    int rc = regcomp(re, source, cflags);
    deallocate(alloc, source, length + 1);

    if (rc) {
        char message[128];
        regerror(rc, re, message, sizeof(message));
        deallocate(alloc, re, sizeof(*re));
        return PSTRTHROWF(PSTRING_EINVAL, "posix: %s", message);
    }

    info->groups = re->re_nsub;
    info->props = 0;
    *prog = re;
    return PSTRING_OK;
}

PSTR_INLINE void pstrexpr_posix_destroy(
    void *prog, allocator_t *alloc, void *user
) {
    (void)user;
    regfree((regex_t *)prog);
    deallocate(alloc, prog, sizeof(regex_t));
}

PSTR_INLINE int pstrexpr_posix_exec(
    void *prog,
    const char *subject,
    size_t length,
    size_t start,
    pstrexpr_span_t *spans,
    size_t nspans,
    void *user
) {
    (void)user;

    regex_t *re = prog;
    regmatch_t stack[PSTREXPR_POSIX_STACK];
    regmatch_t *match = stack;

    if (nspans > PSTREXPR_POSIX_STACK) {
        match = malloc(nspans * sizeof(*match));
        if (!match)
            return PSTRING_ENOMEM;
    }

    int eflags = start > 0 ? REG_NOTBOL : 0;
    size_t shift = 0;
    char *copy = NULL;
    int rc;

#ifdef REG_STARTEND
    match[0].rm_so = (regoff_t)start;
    match[0].rm_eo = (regoff_t)length;
    rc = regexec(re, subject, nspans, match, eflags | REG_STARTEND);
#else
    copy = malloc(length - start + 1);
    if (!copy) {
        if (match != stack)
            free(match);
        return PSTRING_ENOMEM;
    }

    memcpy(copy, subject + start, length - start);
    copy[length - start] = '\0';
    shift = start;
    rc = regexec(re, copy, nspans, match, eflags);
#endif

    free(copy);

    if (rc == 0) {
        for (size_t i = 0; i < nspans; i++) {
            if (match[i].rm_so < 0) {
                spans[i].start = spans[i].end = PSTREXPR_NOSPAN;
            } else {
                spans[i].start = (size_t)match[i].rm_so + shift;
                spans[i].end = (size_t)match[i].rm_eo + shift;
            }
        }
    }

    if (match != stack)
        free(match);

    if (rc == 0)
        return PSTRING_TRUE;
    if (rc == REG_NOMATCH)
        return PSTRING_FALSE;
    return rc == REG_ESPACE ? PSTRING_ENOMEM : PSTRING_EINVAL;
}

/** The vtable, in case you want to register it under a different name. */
static const struct pstrexpr_vt pstrexpr_posix_vt = {
    "posix",
    PSTREXPR_REGEX,
    '\\',
    pstrexpr_posix_compile,
    pstrexpr_posix_destroy,
    pstrexpr_posix_exec,
    NULL,
    NULL,
};

/** Registers POSIX regex under `prefix`, "posix" if it's `NULL`.
    Errors: see `pstrexpr_register`.
*/
PSTR_INLINE int pstrexpr_register_posix(const char *prefix) {
    return pstrexpr_register(
        prefix ? prefix : "posix", &pstrexpr_posix_vt, NULL
    );
}

#endif /* PSTRING_PATTERN_POSIX_H */
