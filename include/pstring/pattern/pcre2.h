/*  pstring - fully-featured string library for C

    Copyright 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_PATTERN_PCRE2_H
#define PSTRING_PATTERN_PCRE2_H

/** Adapter: PCRE2 (8-bit) engine for `<pstring/pattern.h>`.

    This is a single header with no link-time dependency on pstring besides
    the public API. Include it in the translation unit which initializes your
    application (and link against PCRE2 yourself), then register the engine:

        #include <pstring/pattern/pcre2.h>

        pstrexpr_register_pcre2(NULL);   // "pcre/.../", and "re/.../"

        pstrmatch("pcre/(?<year>\\d{4})-\\d\\d/", PSTR("on 2026-03"), &caps);

    Flags:
        i  caseless              m  multiline (^ and $ match at lines)
        s  dot matches newline   x  extended (ignore whitespace)
        u  UTF-8 and Unicode properties (\d, \w, ...)
        U  ungreedy by default   A  anchored at the start offset
        D  `$` matches only at the very end
        n  no automatic captures J  allow duplicate group names

    Unless `PSTREXPR_PCRE2_NO_JIT` is defined, patterns are JIT compiled when
    PCRE2 supports it. Named groups are available in replacements: `${year}`.
    A match data block is allocated on every search; this is where a
    thread-local cache could be added if profiling shows it's needed.
*/

#include <pstring/core.h>
#include <pstring/pattern.h>

#include <stdint.h>
#include <string.h>

#ifndef PCRE2_CODE_UNIT_WIDTH
    #define PCRE2_CODE_UNIT_WIDTH 8
#endif

#if PCRE2_CODE_UNIT_WIDTH != 8
    #error "pstring/pattern/pcre2.h supports only the 8-bit PCRE2 library"
#endif

#include <pcre2.h>

PSTR_INLINE int pstrexpr_pcre2_compile(
    void **prog,
    struct pstrexpr_info *info,
    const pstring_t *pattern,
    const pstring_t *flags,
    allocator_t *alloc,
    void *user
) {
    (void)alloc;
    (void)user;

    uint32_t options = 0;
    for (size_t i = 0; i < pstrlen(flags); i++) {
        switch (pstrget(flags, i)) {
        case 'i':
            options |= PCRE2_CASELESS;
            break;
        case 'm':
            options |= PCRE2_MULTILINE;
            break;
        case 's':
            options |= PCRE2_DOTALL;
            break;
        case 'x':
            options |= PCRE2_EXTENDED;
            break;
        case 'u':
            options |= PCRE2_UTF | PCRE2_UCP;
            break;
        case 'U':
            options |= PCRE2_UNGREEDY;
            break;
        case 'A':
            options |= PCRE2_ANCHORED;
            break;
        case 'D':
            options |= PCRE2_DOLLAR_ENDONLY;
            break;
        case 'n':
            options |= PCRE2_NO_AUTO_CAPTURE;
            break;
        case 'J':
            options |= PCRE2_DUPNAMES;
            break;
        default:
            return PSTRTHROW(PSTRING_EINVAL, "pcre2: unknown flag");
        }
    }

    int error = 0;
    PCRE2_SIZE offset = 0;
    pcre2_code *code = pcre2_compile(
        (PCRE2_SPTR)pstrbuf(pattern),
        pstrlen(pattern),
        options,
        &error,
        &offset,
        NULL
    );

    if (!code) {
        PCRE2_UCHAR message[256];
        if (pcre2_get_error_message(error, message, sizeof(message)) < 0)
            message[0] = '\0';
        return PSTRTHROWF(
            PSTRING_EINVAL, "pcre2: %s (offset %zu)", message, (size_t)offset
        );
    }

#ifndef PSTREXPR_PCRE2_NO_JIT
    (void)pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
#endif

    uint32_t groups = 0;
    pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &groups);

    info->groups = groups;
    info->props = (options & PCRE2_UTF) ? PSTREXPR_UTF8 : 0;
    *prog = code;
    return PSTRING_OK;
}

PSTR_INLINE void pstrexpr_pcre2_destroy(
    void *prog, allocator_t *alloc, void *user
) {
    (void)alloc;
    (void)user;
    pcre2_code_free((pcre2_code *)prog);
}

PSTR_INLINE int pstrexpr_pcre2_exec(
    void *prog,
    const char *subject,
    size_t length,
    size_t start,
    pstrexpr_span_t *spans,
    size_t nspans,
    void *user
) {
    (void)user;

    pcre2_code *code = prog;
    pcre2_match_data *data = pcre2_match_data_create_from_pattern(code, NULL);
    if (!data)
        return PSTRING_ENOMEM;

    int rc = pcre2_match(
        code, (PCRE2_SPTR)subject, length, start, 0, data, NULL
    );

    if (rc == PCRE2_ERROR_NOMATCH) {
        pcre2_match_data_free(data);
        return PSTRING_FALSE;
    }

    if (rc < 0) {
        pcre2_match_data_free(data);

        switch (rc) {
        case PCRE2_ERROR_NOMEMORY:
            return PSTRING_ENOMEM;
        case PCRE2_ERROR_MATCHLIMIT:
        case PCRE2_ERROR_DEPTHLIMIT:
#ifdef PCRE2_ERROR_HEAPLIMIT
        case PCRE2_ERROR_HEAPLIMIT:
#endif
            return PSTRTHROW(PSTRING_ERANGE, "pcre2: match limit exceeded");
        default:
            return PSTRTHROWF(PSTRING_EINVAL, "pcre2: error %d", rc);
        }
    }

    const PCRE2_SIZE *vector = pcre2_get_ovector_pointer(data);
    uint32_t count = pcre2_get_ovector_count(data);

    for (size_t i = 0; i < nspans; i++) {
        if (i >= count || vector[2 * i] == PCRE2_UNSET) {
            spans[i].start = spans[i].end = PSTREXPR_NOSPAN;
            continue;
        }

        spans[i].start = vector[2 * i];
        spans[i].end = vector[2 * i + 1];

        /* `\K` inside a lookahead can move the start past the end */
        if (spans[i].start > spans[i].end)
            spans[i].start = spans[i].end;
    }

    pcre2_match_data_free(data);
    return PSTRING_TRUE;
}

PSTR_INLINE int pstrexpr_pcre2_group(
    void *prog, const pstring_t *name, void *user
) {
    (void)user;

    char buffer[128];
    if (pstrdump(name, buffer, sizeof(buffer)))
        return PSTRING_ENOENT;

    int index = pcre2_substring_number_from_name(
        (pcre2_code *)prog, (PCRE2_SPTR)buffer
    );

    if (index == PCRE2_ERROR_NOUNIQUESUBSTRING)
        return PSTRING_EINVAL;
    return index < 0 ? PSTRING_ENOENT : index;
}

/** The vtable, in case you want to register it under a different name. */
static const struct pstrexpr_vt pstrexpr_pcre2_vt = {
    "pcre2",
    PSTREXPR_REGEX,
    '\\',
    pstrexpr_pcre2_compile,
    pstrexpr_pcre2_destroy,
    pstrexpr_pcre2_exec,
    pstrexpr_pcre2_group,
    NULL,
};

/** Registers PCRE2 under `prefix`, "pcre" if it's `NULL`.
    Errors: see `pstrexpr_register`.
*/
PSTR_INLINE int pstrexpr_register_pcre2(const char *prefix) {
    return pstrexpr_register(
        prefix ? prefix : "pcre", &pstrexpr_pcre2_vt, NULL
    );
}

#endif /* PSTRING_PATTERN_PCRE2_H */
