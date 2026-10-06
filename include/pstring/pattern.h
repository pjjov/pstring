/*  pstring - fully-featured string library for C

    Copyright 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_PATTERN_H
#define PSTRING_PATTERN_H

/** Module: Pattern matching with pluggable engines.

    This module doesn't implement any pattern matching algorithm itself.
    Instead, it provides a common interface on top of which existing libraries
    (PCRE2, POSIX regex, Oniguruma, ...) are plugged in at runtime, using
    thin adapters (see `<pstring/pattern/pcre2.h>` and friends). The library
    never links against any of them: the application includes the adapter
    it needs and registers it during initialization.

        #include <pstring/pattern.h>
        #include <pstring/pattern/pcre2.h>

        pstrexpr_register_pcre2(NULL); // registers "pcre" (and "re")

        pstrarray_t caps = { 0 };
        if (pstrmatch("pcre/(\\w+)@(\\w+)/i", str, &caps) == PSTRING_TRUE)
            ...
        pstrarray_free(&caps);

    ## PATTERN SYNTAX

    Patterns are strings with the following layout, inspired by PCRE/PHP:

        <prefix><delimiter><body><delimiter><flags>

        "pcre/[a-z]+\\d/im"     engine "pcre", body `[a-z]+\d`, flags "im"
        "lua:%d+:"              engine "lua",  body `%d+`,      no flags
        "re{a{2,3}}x"           brackets nest, so no escaping is needed
        "/foo/i"                empty prefix, same as "re/foo/i"

    - `prefix` is the name an engine was registered with (letters, digits and
      underscores). If it's empty, the generic prefix "re" is used.
    - `delimiter` is any printable ASCII character that isn't alphanumeric,
      whitespace, `_`, `\` or the engine's escape character. The opening
      brackets `(`, `[`, `{` and `<` are closed by their counterparts.
    - `body` is handed to the engine verbatim. To include the closing
      delimiter in it, prefix it with the engine's escape character (`\` for
      most engines, `%` for Lua). The escape sequence is NOT removed, so
      engines must accept an escaped punctuation character as a literal.
    - `flags` is whatever follows the closing delimiter. Its meaning is
      defined by the engine, but single letters are the convention (`i`, `m`,
      `s`, `x`, `u`, ...). Unknown flags should be rejected by the engine.

    The generic prefix "re" is bound to the first registered engine that is
    marked as `PSTREXPR_REGEX`. It can be rebound using `pstrexpr_alias`.

    ## CAPTURES

    Operations that report captures fill a `pstrarray_t`. Item `0` is the
    whole match, items `1..n` are capture groups. Items are slices of the
    searched string, so they don't need to be freed individually and are only
    valid while that string is alive and unmoved (watch out for SSO strings).
    The array itself may be freed with `pstrarray_free`.

    A group that didn't take part in the match is a zeroed `pstring_t`, which
    can be told apart from an empty match using `pstrexpr_isset`.

    The array may be a zeroed `pstrarray_t` (it is allocated on demand, using
    its allocator or the standard one), or an array with a user supplied
    buffer, in which case `PSTRING_ERANGE` is returned if it's too small.
    Old items are discarded without being freed.

    ## EMPTY MATCHES

    Operations that find all matches (`iter`, `split`, `repl`) continue after
    an empty match by skipping one character (a whole UTF-8 sequence if the
    engine reports `PSTREXPR_UTF8`). A non-empty match may follow an empty
    one at the next position, as in Python 3.7+ and JavaScript.

    ## THREAD SAFETY

    The registry and the pattern cache are protected by a mutex, and compiled
    patterns are reference counted, so all functions can be called from
    multiple threads. A compiled `pstrexpr_t` may be shared between threads
    provided the engine's `exec` is reentrant, which holds for all bundled
    adapters.

    Locking and counting are built on `pf_threads.h` and `pf_atomic.h`, which
    use C11 `<threads.h>` and `<stdatomic.h>` where available and fall back to
    POSIX or Windows threads and GNU C builtins otherwise. Without any atomics
    the counts are guarded by the same mutex. Define `PSTREXPR_SINGLE_THREADED`
    (or `PF_NO_PTHREAD`, for POSIX) to drop all locking.
*/

#ifndef PSTR_API
    #define PSTR_API
#endif

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#include <pstring/core.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct allocator_t allocator_t;
typedef struct pstring_t pstring_t;
typedef struct pstrarray_t pstrarray_t;
typedef struct pstrexpr_t pstrexpr_t;

/** Number of compiled patterns kept by the cached variants (LRU eviction). */
#ifndef PSTREXPR_CACHE_SIZE
    #define PSTREXPR_CACHE_SIZE 32
#endif

/** Value of `start` and `end` of an unset (non-participating) group. */
#define PSTREXPR_NOSPAN ((size_t)-1)

/** Engine properties, set in `pstrexpr_vt.flags`. */
enum pstrexpr_flag {
    /** The engine is a regular expression flavour. The first one registered
        becomes the target of the generic prefix "re". */
    PSTREXPR_REGEX = 1,
};

/** Properties of a compiled pattern, set by `pstrexpr_vt.compile`. */
enum pstrexpr_prop {
    /** The pattern operates on UTF-8 code points, not bytes, so empty matches
        are skipped by whole characters. */
    PSTREXPR_UTF8 = 1,
};

/** Byte range `[start, end)` inside the searched string. */
typedef struct pstrexpr_span {
    size_t start;
    size_t end;
} pstrexpr_span_t;

/** Information about a pattern, reported by `pstrexpr_vt.compile`. */
struct pstrexpr_info {
    /** Number of capture groups, not counting the whole match (group 0). */
    size_t groups;
    /** Bitmask of `enum pstrexpr_prop`. */
    unsigned props;
};

/** Callback for `pstriterp` and `pstrexpr_iter`. `groups` holds the captures
    of the match number `index` (see CAPTURES). The array and its items are
    only valid during the call.

    Returns `0` to continue, a positive value to stop iterating, or
    a negative `pstring_error` to abort with that error.
*/
typedef int(pstriterp_fn)(const pstrarray_t *groups, size_t index, void *user);

/** Pattern matching engine vtable.

    The vtable and `user` must outlive every pattern compiled with them
    (static storage is the usual choice). Strings handed to the callbacks
    are not null-terminated; use `pstrterm` if you need that.
    All callbacks should return `PSTRING_*` error codes.
*/
struct pstrexpr_vt {
    /** Human-readable name, optional. */
    const char *name;

    /** Bitmask of `enum pstrexpr_flag`. */
    unsigned flags;

    /** Character used to escape the delimiter inside patterns, `\` if zero.
        This must be a printable ASCII character that isn't alphanumeric. */
    int escape;

    /** Compiles `pattern` using `flags` and stores the engine's handle in
        `prog`. The number of capture groups and the pattern's properties
        must be stored in `info`. If possible, memory should be taken from
        `alloc`, which is never `NULL`.

        Returns `PSTRING_OK`, `PSTRING_EINVAL` for a malformed pattern
        or flags, or `PSTRING_ENOMEM`.
    */
    int (*compile)(
        void **prog,
        struct pstrexpr_info *info,
        const pstring_t *pattern,
        const pstring_t *flags,
        allocator_t *alloc,
        void *user
    );

    /** Releases a handle created by `compile`. */
    void (*destroy)(void *prog, allocator_t *alloc, void *user);

    /** Searches `subject` for the first match starting at or after byte
        `start`. Anchors and look-behinds must still see the whole subject.

        On a match, exactly `nspans` (the number of groups plus one) elements
        of `spans` must be filled, using `PSTREXPR_NOSPAN` for unset groups.

        Returns `PSTRING_TRUE`, `PSTRING_FALSE` if there is no match or
        a negative error (e.g. a match limit was hit).
        Must be callable concurrently for the same `prog`.
    */
    int (*exec)(
        void *prog,
        const char *subject,
        size_t length,
        size_t start,
        pstrexpr_span_t *spans,
        size_t nspans,
        void *user
    );

    /** Optional. Resolves the name of a capture group (as used in `${name}`
        replacement references) to its index.

        Returns the index or `PSTRING_ENOENT`.
    */
    int (*group)(void *prog, const pstring_t *name, void *user);

    /** Optional. Appends `text` to `out`, escaped so that the engine
        matches it literally, escaping only what is necessary (for engines
        where escaping a harmless character changes its meaning, such as
        `\<` in GNU regex). A generic escape that precedes every punctuation
        character with `escape` is used if this is `NULL`.
    */
    int (*quote)(pstring_t *out, const pstring_t *text, void *user);
};

/** Registers an engine under the name `prefix`. If the engine is marked
    `PSTREXPR_REGEX` and the prefix "re" is not taken, "re" will refer to it.

    The same prefix can't be registered twice, except for "re" when it's only
    an alias. Registration is meant to happen during initialization.

    Errors: EINVAL, EEXIST, ENOMEM.
*/
PSTR_API int pstrexpr_register(
    const char *prefix, const struct pstrexpr_vt *vtable, void *user
);

/** Makes `alias` an alternative name of the engine registered as `prefix`,
    replacing a previous alias with that name. This can be used to select
    which engine the generic prefix "re" refers to.

    Errors: EINVAL, ENOENT, EEXIST (`alias` is a real engine), ENOMEM.
*/
PSTR_API int pstrexpr_alias(const char *alias, const char *prefix);

/** Drops all patterns held by the cache of `pstrmatch` and friends. */
PSTR_API void pstrexpr_cache_clear(void);

/** Checks if the capture `group` took part in the match. */
PSTR_INLINE int pstrexpr_isset(const pstring_t *group) {
    return group != NULL && group->buffer != NULL;
}

/* Cached patterns
   These variants compile `pattern` on first use and keep the result in a small
   LRU cache, so they are convenient for patterns that appear as literals
   in the code. Prefer `pstrexpr_compile` for long-lived or hot patterns. */

/** Searches `src` for the first match of `pattern`. If `captures` is not
    `NULL`, it receives the groups (see CAPTURES), or is emptied if there
    is no match.

    Returns `PSTRING_TRUE`, `PSTRING_FALSE` or a negative error:
    EINVAL (malformed pattern or arguments), ENOENT (unknown engine),
    ENOMEM, ERANGE, and any error reported by the engine.
*/
PSTR_API int pstrmatch(
    const char *pattern, const pstring_t *src, pstrarray_t *captures
);

/** Splits `src` on every match of `pattern`, storing the pieces (slices of
    `src`) in `parts`. At most `maxSplit` splits are done (`0` means no
    limit), so there are at most `maxSplit + 1` pieces. Capture groups are
    not included. Empty matches at the very start or end are ignored.

    Returns the number of pieces (at least one) or a negative error.
*/
PSTR_API int pstrsplitp(
    const char *pattern,
    const pstring_t *src,
    pstrarray_t *parts,
    size_t maxSplit
);

/** Replaces at most `max` matches of `pattern` in `str` with `repl`
    (`0` means all of them). The string must be owned (not a slice) and is
    left untouched on failure.

    The replacement may contain references to groups: `$0` (whole match) to
    `$9`, `${12}`, `${name}` (if the engine supports it) and `$$` for a
    literal `$`. Groups that didn't take part in the match are empty.

    Returns the number of replacements or a negative error.
*/
PSTR_API int pstrreplp(
    const char *pattern, const char *repl, pstring_t *str, size_t max
);

/** Calls `iter` for every match of `pattern` in `src`, until it asks
    to stop. Returns the number of matches visited or a negative error.
*/
PSTR_API int pstriterp(
    const char *pattern, const pstring_t *src, pstriterp_fn *iter, void *user
);

/* Precompiled patterns */

/** Compiles `pattern` and stores it in `out`, which must be released with
    `pstrexpr_free`. If `alloc` is `NULL`, the standard allocator is used.

    Errors: EINVAL, ENOENT, ENOMEM, and any error reported by the engine.
*/
PSTR_API int pstrexpr_compile(
    pstrexpr_t **out, const char *pattern, allocator_t *alloc
);

/** Releases a pattern created by `pstrexpr_compile`. */
PSTR_API void pstrexpr_free(pstrexpr_t *expr);

/** Returns the number of capture groups, not counting the whole match. */
PSTR_API size_t pstrexpr_groups(const pstrexpr_t *expr);

/** Variants of `pstrmatch`, `pstrsplitp`, `pstrreplp` and `pstriterp` which
    take a precompiled pattern.
*/
PSTR_API int pstrexpr_match(
    const pstrexpr_t *expr, const pstring_t *src, pstrarray_t *captures
);
PSTR_API int pstrexpr_split(
    const pstrexpr_t *expr,
    const pstring_t *src,
    pstrarray_t *parts,
    size_t maxSplit
);
PSTR_API int pstrexpr_repl(
    const pstrexpr_t *expr, const char *repl, pstring_t *str, size_t max
);
PSTR_API int pstrexpr_iter(
    const pstrexpr_t *expr, const pstring_t *src, pstriterp_fn *iter, void *user
);

/** Appends `text` to `out`, escaped so that the engine registered as
    `prefix` (the generic one if `NULL` or empty) matches it literally. The
    result is the body of a pattern: the delimiters and flags are not added.

    Unless the engine has its own `quote` callback, every ASCII punctuation or
    whitespace character is preceded by the engine's escape character, which
    also protects the delimiter whatever it is. Engines with their own
    callback only escape what they must, so the body is only safe inside a
    delimiter that doesn't occur in it. Prefer `pstrexpr_quote` for building
    patterns from untrusted text.

    Errors: EINVAL, ENOENT, ENOMEM.
*/
PSTR_API int pstrexpr_escape(
    pstring_t *out, const char *prefix, const pstring_t *text
);

/** Appends a complete pattern to `out` that matches `text` literally,
    including the prefix, a delimiter which is guaranteed not to occur in the
    body, and the given `flags` (optional). The result is safe to use with the
    functions above, no matter what `text` contains:

        pstring_t pat = { 0 };
        pstrexpr_quote(&pat, "pcre", user_input, "i");
        pstrmatch(pstrbuf(&pat), haystack, NULL);
        pstrfree(&pat);

    On failure `out` is left as it was.

    Errors: EINVAL, ENOENT, ENOMEM, ERANGE (no usable delimiter).
*/
PSTR_API int pstrexpr_quote(
    pstring_t *out, const char *prefix, const pstring_t *text, const char *flags
);

#ifdef __cplusplus
}
#endif

#endif /* PSTRING_PATTERN_H */
