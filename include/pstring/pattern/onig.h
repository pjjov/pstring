/*  pstring - fully-featured string library for C

    Copyright 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_PATTERN_ONIG_H
#define PSTRING_PATTERN_ONIG_H

/** Adapter: Oniguruma (UTF-8, Perl syntax with named groups) engine for
    `<pstring/pattern.h>`.

        #include <pstring/pattern/onig.h>

        pstrexpr_register_onig(NULL);   // "onig/.../", and "re/.../"

    Flags:
        i  case-insensitive      s  dot matches newline
        m  `^` and `$` match at lines (Perl semantics)
        x  extended (ignore whitespace)
        R  use Ruby syntax instead of Perl

    Plain and named groups are both captured, like in PCRE (Oniguruma's own
    default is to ignore plain groups in patterns that have named ones).
    Patterns operate on UTF-8 code points. Named groups are available in
    replacements: `${name}`. If a name is shared by several groups, the last
    one is used.
*/

#include <pstring/core.h>
#include <pstring/pattern.h>

#include <oniguruma.h>

#include <string.h>

PSTR_INLINE int pstrexpr_onig_error(int code, OnigErrorInfo *info) {
    UChar message[ONIG_MAX_ERROR_MESSAGE_LEN];
    onig_error_code_to_str(message, code, info);
    return PSTRTHROWF(PSTRING_EINVAL, "onig: %s", (const char *)message);
}

PSTR_INLINE int pstrexpr_onig_compile(
    void **prog,
    struct pstrexpr_info *info,
    const pstring_t *pattern,
    const pstring_t *flags,
    allocator_t *alloc,
    void *user
) {
    (void)alloc;
    (void)user;

    /* by default, plain groups stop capturing once a named one is present */
    OnigOptionType options = ONIG_OPTION_CAPTURE_GROUP;
    OnigSyntaxType *syntax = ONIG_SYNTAX_PERL_NG;

    for (size_t i = 0; i < pstrlen(flags); i++) {
        switch (pstrget(flags, i)) {
        case 'i':
            options |= ONIG_OPTION_IGNORECASE;
            break;
        case 'm':
            options |= ONIG_OPTION_NEGATE_SINGLELINE;
            break;
        case 's':
            options |= ONIG_OPTION_MULTILINE;
            break;
        case 'x':
            options |= ONIG_OPTION_EXTEND;
            break;
        case 'R':
            syntax = ONIG_SYNTAX_RUBY;
            break;
        default:
            return PSTRTHROW(PSTRING_EINVAL, "onig: unknown flag");
        }
    }

    regex_t *reg = NULL;
    OnigErrorInfo error;
    const UChar *start = (const UChar *)pstrbuf(pattern);

    int rc = onig_new(
        &reg,
        start,
        start + pstrlen(pattern),
        options,
        ONIG_ENCODING_UTF8,
        syntax,
        &error
    );
    if (rc != ONIG_NORMAL)
        return pstrexpr_onig_error(rc, &error);

    info->groups = (size_t)onig_number_of_captures(reg);
    info->props = PSTREXPR_UTF8;
    *prog = reg;
    return PSTRING_OK;
}

PSTR_INLINE void pstrexpr_onig_destroy(
    void *prog, allocator_t *alloc, void *user
) {
    (void)alloc;
    (void)user;
    onig_free((regex_t *)prog);
}

PSTR_INLINE int pstrexpr_onig_exec(
    void *prog,
    const char *subject,
    size_t length,
    size_t start,
    pstrexpr_span_t *spans,
    size_t nspans,
    void *user
) {
    (void)user;

    OnigRegion *region = onig_region_new();
    if (!region)
        return PSTRING_ENOMEM;

    const UChar *begin = (const UChar *)subject;
    const UChar *end = begin + length;

    int rc = onig_search(
        (regex_t *)prog,
        begin,
        end,
        begin + start,
        end,
        region,
        ONIG_OPTION_NONE
    );

    if (rc >= 0) {
        for (size_t i = 0; i < nspans; i++) {
            if (i >= (size_t)region->num_regs || region->beg[i] < 0) {
                spans[i].start = spans[i].end = PSTREXPR_NOSPAN;
            } else {
                spans[i].start = (size_t)region->beg[i];
                spans[i].end = (size_t)region->end[i];
            }
        }
    }

    onig_region_free(region, 1);

    if (rc >= 0)
        return PSTRING_TRUE;
    if (rc == ONIG_MISMATCH)
        return PSTRING_FALSE;
    if (rc == ONIGERR_MEMORY)
        return PSTRING_ENOMEM;

    return pstrexpr_onig_error(rc, NULL);
}

PSTR_INLINE int pstrexpr_onig_group(
    void *prog, const pstring_t *name, void *user
) {
    (void)user;

    const UChar *begin = (const UChar *)pstrbuf(name);
    int index = onig_name_to_backref_number(
        (regex_t *)prog, begin, begin + pstrlen(name), NULL
    );
    return index < 0 ? PSTRING_ENOENT : index;
}

/** The vtable, in case you want to register it under a different name. */
static const struct pstrexpr_vt pstrexpr_onig_vt = {
    "onig",
    PSTREXPR_REGEX,
    '\\',
    pstrexpr_onig_compile,
    pstrexpr_onig_destroy,
    pstrexpr_onig_exec,
    pstrexpr_onig_group,
    NULL,
};

/** Initializes Oniguruma for UTF-8 and registers it under `prefix`,
    "onig" if it's `NULL`. Errors: see `pstrexpr_register`.

    > libonig exports its own POSIX-style `regcomp` and `regexec`. In a program
    > that links it, these replace the C library's, which silently breaks
    > `<pstring/pattern/posix.h>` (it would run Oniguruma's BRE/ERE emulation).
    > Don't use the two adapters in the same program.
*/
PSTR_INLINE int pstrexpr_register_onig(const char *prefix) {
    OnigEncoding encodings[] = { ONIG_ENCODING_UTF8 };
    if (onig_initialize(encodings, 1) != ONIG_NORMAL)
        return PSTRING_EINVAL;

    return pstrexpr_register(prefix ? prefix : "onig", &pstrexpr_onig_vt, NULL);
}

#endif /* PSTRING_PATTERN_ONIG_H */
