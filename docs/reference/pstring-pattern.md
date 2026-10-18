# Pattern matching with pluggable engines.

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

### PATTERN SYNTAX

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

### CAPTURES

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

### EMPTY MATCHES

Operations that find all matches (`iter`, `split`, `repl`) continue after
an empty match by skipping one character (a whole UTF-8 sequence if the
engine reports `PSTREXPR_UTF8`). A non-empty match may follow an empty
one at the next position, as in Python 3.7+ and JavaScript.

### THREAD SAFETY

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

- [`PSTREXPR_CACHE_SIZE`](#pstrexpr_cache_size) — Number of compiled patterns kept by the cached variants (LRU eviction).
- [`PSTREXPR_NOSPAN`](#pstrexpr_nospan) — Value of `start` and `end` of an unset (non-participating) group.
- [`pstrexpr_flag`](#pstrexpr_flag) — Engine properties, set in `pstrexpr_vt.flags`.
- [`pstrexpr_prop`](#pstrexpr_prop) — Properties of a compiled pattern, set by `pstrexpr_vt.compile`.
- [`pstrexpr_span_t`](#pstrexpr_span_t) — Byte range `[start, end)` inside the searched string.
- [`pstrexpr_info`](#pstrexpr_info) — Information about a pattern, reported by `pstrexpr_vt.compile`.
- [`pstriterp_fn`](#pstriterp_fn) — Callback for `pstriterp` and `pstrexpr_iter`.
- [`pstrexpr_vt`](#pstrexpr_vt) — Pattern matching engine vtable.
- [`pstrexpr_register`](#pstrexpr_register) — Registers an engine under the name `prefix`.
- [`pstrexpr_alias`](#pstrexpr_alias) — Makes `alias` an alternative name of the engine registered as `prefix`, replacing a previous alias with that name.
- [`pstrexpr_cache_clear`](#pstrexpr_cache_clear) — Drops all patterns held by the cache of `pstrmatch` and friends.
- [`pstrexpr_isset`](#pstrexpr_isset) — Checks if the capture `group` took part in the match.
- [`pstrmatch`](#pstrmatch) — Searches `src` for the first match of `pattern`.
- [`pstrsplitp`](#pstrsplitp) — Splits `src` on every match of `pattern`, storing the pieces (slices of `src`) in `parts`.
- [`pstrreplp`](#pstrreplp) — Replaces at most `max` matches of `pattern` in `str` with `repl` (`0` means all of them).
- [`pstriterp`](#pstriterp) — Calls `iter` for every match of `pattern` in `src`, until it asks to stop.
- [`pstrexpr_compile`](#pstrexpr_compile) — Compiles `pattern` and stores it in `out`, which must be released with `pstrexpr_free`.
- [`pstrexpr_free`](#pstrexpr_free) — Releases a pattern created by `pstrexpr_compile`.
- [`pstrexpr_groups`](#pstrexpr_groups) — Returns the number of capture groups, not counting the whole match.
- [`pstrexpr_match`](#pstrexpr_match) — Variants of `pstrmatch`, `pstrsplitp`, `pstrreplp` and `pstriterp` which take a precompiled pattern.
- [`pstrexpr_escape`](#pstrexpr_escape) — Appends `text` to `out`, escaped so that the engine registered as `prefix` (the generic one if `NULL` or empty) matches it literally.
- [`pstrexpr_quote`](#pstrexpr_quote) — Appends a complete pattern to `out` that matches `text` literally, including the prefix, a delimiter which is guaranteed not to occur in the body, and the given `flags` (optional).

<a id="pstrexpr_cache_size"></a>

## `PSTREXPR_CACHE_SIZE`

```c
#define PSTREXPR_CACHE_SIZE 32
```

Number of compiled patterns kept by the cached variants (LRU eviction).

<a id="pstrexpr_nospan"></a>

## `PSTREXPR_NOSPAN`

```c
#define PSTREXPR_NOSPAN ((size_t)-1)
```

Value of `start` and `end` of an unset (non-participating) group.

<a id="pstrexpr_flag"></a>

## `pstrexpr_flag`

```c
enum pstrexpr_flag {

    PSTREXPR_REGEX = 1,
};
```

Engine properties, set in `pstrexpr_vt.flags`.

<a id="pstrexpr_prop"></a>

## `pstrexpr_prop`

```c
enum pstrexpr_prop {

    PSTREXPR_UTF8 = 1,
};
```

Properties of a compiled pattern, set by `pstrexpr_vt.compile`.

<a id="pstrexpr_span_t"></a>

## `pstrexpr_span_t`

```c
typedef struct pstrexpr_span {
    size_t start;
    size_t end;
} pstrexpr_span_t;
```

Byte range `[start, end)` inside the searched string.

<a id="pstrexpr_info"></a>

## `pstrexpr_info`

```c
struct pstrexpr_info {

    size_t groups;

    unsigned props;
};
```

Information about a pattern, reported by `pstrexpr_vt.compile`.

<a id="pstriterp_fn"></a>

## `pstriterp_fn`

```c
typedef int(pstriterp_fn)(const pstrarray_t *groups, size_t index, void *user);
```

Callback for `pstriterp` and `pstrexpr_iter`. `groups` holds the captures
of the match number `index` (see CAPTURES). The array and its items are
only valid during the call.

Returns `0` to continue, a positive value to stop iterating, or
a negative `pstring_error` to abort with that error.

<a id="pstrexpr_vt"></a>

## `pstrexpr_vt`

```c
struct pstrexpr_vt {

    const char *name;


    unsigned flags;


    int escape;


    int (*compile)(
        void **prog,
        struct pstrexpr_info *info,
        const pstring_t *pattern,
        const pstring_t *flags,
        allocator_t *alloc,
        void *user
    );


    void (*destroy)(void *prog, allocator_t *alloc, void *user);


    int (*exec)(
        void *prog,
        const char *subject,
        size_t length,
        size_t start,
        pstrexpr_span_t *spans,
        size_t nspans,
        void *user
    );


    int (*group)(void *prog, const pstring_t *name, void *user);


    int (*quote)(pstring_t *out, const pstring_t *text, void *user);
};
```

Pattern matching engine vtable.

The vtable and `user` must outlive every pattern compiled with them
(static storage is the usual choice). Strings handed to the callbacks
are not null-terminated; use `pstrterm` if you need that.
All callbacks should return `PSTRING_*` error codes.

<a id="pstrexpr_register"></a>

## `pstrexpr_register`

```c
int pstrexpr_register(
    const char *prefix, const struct pstrexpr_vt *vtable, void *user
);
```

Registers an engine under the name `prefix`. If the engine is marked
`PSTREXPR_REGEX` and the prefix "re" is not taken, "re" will refer to it.

The same prefix can't be registered twice, except for "re" when it's only
an alias. Registration is meant to happen during initialization.

**Errors**

EINVAL, EEXIST, ENOMEM.

<a id="pstrexpr_alias"></a>

## `pstrexpr_alias`

```c
int pstrexpr_alias(const char *alias, const char *prefix);
```

Makes `alias` an alternative name of the engine registered as `prefix`,
replacing a previous alias with that name. This can be used to select
which engine the generic prefix "re" refers to.

**Errors**

EINVAL, ENOENT, EEXIST (`alias` is a real engine), ENOMEM.

<a id="pstrexpr_cache_clear"></a>

## `pstrexpr_cache_clear`

```c
void pstrexpr_cache_clear(void);
```

Drops all patterns held by the cache of `pstrmatch` and friends.

<a id="pstrexpr_isset"></a>

## `pstrexpr_isset`

```c
PSTR_INLINE int pstrexpr_isset(const pstring_t *group);
```

Checks if the capture `group` took part in the match.

<a id="pstrmatch"></a>

## `pstrmatch`

```c
int pstrmatch(
    const char *pattern, const pstring_t *src, pstrarray_t *captures
);
```

Searches `src` for the first match of `pattern`. If `captures` is not
`NULL`, it receives the groups (see CAPTURES), or is emptied if there
is no match.

Returns `PSTRING_TRUE`, `PSTRING_FALSE` or a negative error:
EINVAL (malformed pattern or arguments), ENOENT (unknown engine),
ENOMEM, ERANGE, and any error reported by the engine.

<a id="pstrsplitp"></a>

## `pstrsplitp`

```c
int pstrsplitp(
    const char *pattern,
    const pstring_t *src,
    pstrarray_t *parts,
    size_t maxSplit
);
```

Splits `src` on every match of `pattern`, storing the pieces (slices of
`src`) in `parts`. At most `maxSplit` splits are done (`0` means no
limit), so there are at most `maxSplit + 1` pieces. Capture groups are
not included. Empty matches at the very start or end are ignored.

Returns the number of pieces (at least one) or a negative error.

<a id="pstrreplp"></a>

## `pstrreplp`

```c
int pstrreplp(
    const char *pattern, const char *repl, pstring_t *str, size_t max
);
```

Replaces at most `max` matches of `pattern` in `str` with `repl`
(`0` means all of them). The string must be owned (not a slice) and is
left untouched on failure.

The replacement may contain references to groups: `$0` (whole match) to
`$9`, `${12}`, `${name}` (if the engine supports it) and `$$` for a
literal `$`. Groups that didn't take part in the match are empty.

Returns the number of replacements or a negative error.

<a id="pstriterp"></a>

## `pstriterp`

```c
int pstriterp(
    const char *pattern, const pstring_t *src, pstriterp_fn *iter, void *user
);
```

Calls `iter` for every match of `pattern` in `src`, until it asks
to stop. Returns the number of matches visited or a negative error.

<a id="pstrexpr_compile"></a>

## `pstrexpr_compile`

```c
int pstrexpr_compile(
    pstrexpr_t **out, const char *pattern, allocator_t *alloc
);
```

Compiles `pattern` and stores it in `out`, which must be released with
`pstrexpr_free`. If `alloc` is `NULL`, the standard allocator is used.

**Errors**

EINVAL, ENOENT, ENOMEM, and any error reported by the engine.

<a id="pstrexpr_free"></a>

## `pstrexpr_free`

```c
void pstrexpr_free(pstrexpr_t *expr);
```

Releases a pattern created by `pstrexpr_compile`.

<a id="pstrexpr_groups"></a>

## `pstrexpr_groups`

```c
size_t pstrexpr_groups(const pstrexpr_t *expr);
```

Returns the number of capture groups, not counting the whole match.

<a id="pstrexpr_match"></a>

## `pstrexpr_match`

```c
int pstrexpr_match(
    const pstrexpr_t *expr, const pstring_t *src, pstrarray_t *captures
);
int pstrexpr_split(
    const pstrexpr_t *expr,
    const pstring_t *src,
    pstrarray_t *parts,
    size_t maxSplit
);
int pstrexpr_repl(
    const pstrexpr_t *expr, const char *repl, pstring_t *str, size_t max
);
int pstrexpr_iter(
    const pstrexpr_t *expr, const pstring_t *src, pstriterp_fn *iter, void *user
);
```

Variants of `pstrmatch`, `pstrsplitp`, `pstrreplp` and `pstriterp` which
take a precompiled pattern.

<a id="pstrexpr_escape"></a>

## `pstrexpr_escape`

```c
int pstrexpr_escape(
    pstring_t *out, const char *prefix, const pstring_t *text
);
```

Appends `text` to `out`, escaped so that the engine registered as
`prefix` (the generic one if `NULL` or empty) matches it literally. The
result is the body of a pattern: the delimiters and flags are not added.

Unless the engine has its own `quote` callback, every ASCII punctuation or
whitespace character is preceded by the engine's escape character, which
also protects the delimiter whatever it is. Engines with their own
callback only escape what they must, so the body is only safe inside a
delimiter that doesn't occur in it. Prefer `pstrexpr_quote` for building
patterns from untrusted text.

**Errors**

EINVAL, ENOENT, ENOMEM.

<a id="pstrexpr_quote"></a>

## `pstrexpr_quote`

```c
int pstrexpr_quote(
    pstring_t *out, const char *prefix, const pstring_t *text, const char *flags
);
```

Appends a complete pattern to `out` that matches `text` literally,
including the prefix, a delimiter which is guaranteed not to occur in the
body, and the given `flags` (optional). The result is safe to use with the
functions above, no matter what `text` contains:

    pstring_t pat = { 0 };
    pstrexpr_quote(&pat, "pcre", user_input, "i");
    pstrmatch(pstrbuf(&pat), haystack, NULL);
    pstrfree(&pat);

On failure `out` is left as it was.

**Errors**

EINVAL, ENOENT, ENOMEM, ERANGE (no usable delimiter).

