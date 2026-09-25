/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_GLOB_H
#define PSTRING_GLOB_H

#include <pstring/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Module: Cross-platform filename globbing.

    `pstrglob_match` implements POSIX `fnmatch(3)` style matching of a single
    name against a pattern (`*`, `?`, `[...]`, and `[!...]`/`[^...]`), with
    no filesystem access at all - it's a pure string predicate and behaves
    identically on every platform.

    `pstrglob` walks a path pattern component by component (splitting on
    `/`), listing directories with `<dirent.h>` on POSIX and
    `FindFirstFile`/`FindNextFile` on Windows, and matching each component
    against the corresponding pattern segment with `pstrglob_match`. Matches
    are collected depth-first, so a pattern with multiple wildcard segments
    (e.g. `src` then `*` then `*.c`) works the same way it would in a
    POSIX shell.
**/

enum pstrglob_flag {
    /** Match a leading `.` in a filename only when the pattern's
        corresponding component also starts with a literal `.` -- the same
        convention shells use to hide dotfiles from a bare `*`. **/
    PSTRGLOB_PERIOD = 1,
    /** Skip the final `qsort` of results within each directory. Matches
        are still produced in directory-listing order. **/
    PSTRGLOB_NOSORT = 2,
    /** If nothing matches, append the literal pattern to the result
        instead of returning `PSTRING_ENOENT` - mirrors glibc's
        `GLOB_NOCHECK`. **/
    PSTRGLOB_NOCHECK = 4,
    /** Treat `\\` as an ordinary character instead of an escape. Useful
        on Windows, where `\\` is also the path separator. **/
    PSTRGLOB_NOESCAPE = 8,
    /** Append a trailing `/` to results that are themselves directories. **/
    PSTRGLOB_MARK = 16,
};

/** Tests whether `name` matches the shell-style wildcard `pattern`:
    - `*` matches any run of characters (including none),
    - `?` matches exactly one character,
    - `[abc]` matches one of `a`, `b`, or `c`,
    - `[a-z]` matches one character in the given range,
    - `[!...]` or `[^...]` matches any character NOT in the set,
    - `\\x` matches the literal character `x` (unless `PSTRGLOB_NOESCAPE`).

    Unlike `pstrglob`, this function never touches the filesystem, so it
    is safe to use as a general-purpose wildcard matcher (e.g. for the
    `case` patterns during word expansion).

    Returns `PSTRING_TRUE`, `PSTRING_FALSE`, or a negative `pstring_error`.
**/
PSTR_API int pstrglob_match(
    const char *pattern, const pstring_t *name, int flags
);

/** Expands `pattern` into every path that exists on disk and matches it,
    appending results to `dst` (which follows the same ownership rules as
    `pstrarray_t` elsewhere in pstring: initialize with `{0}` and free with
    `pstrarray_free`). Path components are separated by `/` on every
    platform; `\\` is additionally accepted as a separator on Windows.

    If no matches are found, returns `PSTRING_ENOENT` unless
    `PSTRGLOB_NOCHECK` is set, in which case `pattern` itself is appended
    as the sole result.

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_ENOENT.
**/
PSTR_API int pstrglob(
    pstrarray_t *dst, const pstring_t *pattern, int flags, allocator_t *alloc
);

/** Expands shell-style `{a,b,c}` brace notation, appending every
    resulting combination to `dst` (same ownership rules as `pstrglob`:
    initialize with `{0}`, free with `pstrarray_free`). This is a purely
    textual expansion -- it never touches the filesystem and has nothing
    to do with `pstrglob_match`'s wildcards:

        "file.{c,h}"     -> ["file.c", "file.h"]
        "a{b,c{d,e}}f"   -> ["abf", "acdf", "acef"]
        "img{01..03}.png"  (numeric ranges are not supported)

    A `{...}` group is only expanded if it contains at least one
    top-level comma; otherwise its braces are left as literal characters
    (matching the shell convention that `{lonely}` is not a valid brace
    expression). `\\{`, `\\}` and `\\,` are literal characters rather
    than group/separator syntax. Multiple groups in the same pattern
    (adjacent or nested) all expand, and the results are the cross
    product of every group's alternatives.

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrbrace(
    pstrarray_t *dst, const pstring_t *pattern, allocator_t *alloc
);

#ifdef __cplusplus
}
#endif

#endif
