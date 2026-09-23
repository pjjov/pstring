/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_WORDEXP_H
#define PSTRING_WORDEXP_H

#ifndef PSTR_API
    #define PSTR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct pstring_t pstring_t;
typedef struct pstrarray_t pstrarray_t;

/* Hook identifiers for the `pstrexpand_with` function */
enum pstrexpand_hook {
    PSTREXPAND_NONE,
    PSTREXPAND_NAMED,
    PSTREXPAND_BRACE,
    PSTREXPAND_CMD_PAREN,
    PSTREXPAND_CMD_TICK,
    PSTREXPAND_ARITHMETIC,
    PSTREXPAND_TILDE,
    PSTREXPAND_STATUS,
    PSTREXPAND_PID,
    PSTREXPAND_IFS,
    PSTREXPAND_GLOB,
};

/* Flags for `pstrexpand` family of functions */
enum pstrexpand_flags {
    PSTREXPAND_UNDEF = 0x1,
    PSTREXPAND_REUSE = 0x2,
    PSTREXPAND_NOCMD = 0x4,
    PSTREXPAND_APPEND = 0x8,
};

/* Error codes that mirror <wordexp.h> codes */
enum pstrexpand_error {
    PSTREXPAND_BADCHAR = -200,
    PSTREXPAND_BADVAL = -201,
    PSTREXPAND_CMDSUB = -202,
    PSTREXPAND_SYNTAX = -203,
};

/** Callback used for the `wordexp` shell expansion. **/
typedef int(pstrexpand_fn)(
    void *dst, void *src, int flags, int kind, void *user
);

/** Performs word expansion using the default callback. **/
PSTR_API int pstrexpand(
    pstrarray_t *dst, pstring_t *src, int flags, pstrexpand_fn *cb
);

/** Performs word expansion using the provided callback. **/
PSTR_API int pstrexpand_with(
    pstrarray_t *dst, pstring_t *src, int flags, pstrexpand_fn *cb, void *user
);

/** Default callback used for `wordexp` shell expansion. */
PSTR_API int pstrexpand_default_cb(
    void *dst, void *src, int flags, int kind, void *user
);

#ifdef __cplusplus
}
#endif

#endif /* PSTRING_WORDEXP_H */