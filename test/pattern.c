/*  pstring - fully-featured string library for C

    Copyright 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pf_assert.h>
#include <pf_test.h>

#include <pstring/pattern.h>
#include <pstring/pstring.h>

#include <allocator.h>
#include <allocator_std.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* libonig overrides the C library's regcomp, see meson_options.txt */
#if !defined(_WIN32) && !defined(PSTRING_TEST_ONIG)
    #define TEST_POSIX
    #include <pstring/pattern/posix.h>
#endif

#ifdef PSTRING_TEST_PCRE2
    #include <pstring/pattern/pcre2.h>
#endif

#ifdef PSTRING_TEST_ONIG
    #include <pstring/pattern/onig.h>
#endif

#include <pf_threads.h>

#if !defined(PF_THREADS_NONE) && !defined(PSTREXPR_SINGLE_THREADED)
    #define TEST_THREADS
#endif

/* A counting allocator, used to detect leaks in compiled patterns. */

typedef struct counting_t {
    allocator_t base;
    long live;
} counting_t;

static void *counting_fn(
    allocator_t *self, void *ptr, size_t old, size_t size, size_t zalign
) {
    counting_t *counting = (counting_t *)self;
    void *out = standard_allocator_fn(self, ptr, old, size, zalign);

    if (!ptr && out)
        counting->live++;
    else if (ptr && size == 0)
        counting->live--;
    return out;
}

/* Toy engines. "lit" searches for a literal substring using `%` to escape
   characters, which makes it independent of any third-party library. */

typedef struct lit_t {
    char *needle;
    size_t length;
} lit_t;

static int lit_compile(
    void **prog,
    struct pstrexpr_info *info,
    const pstring_t *pattern,
    const pstring_t *flags,
    allocator_t *alloc,
    void *user
) {
    (void)user;

    unsigned props = 0;
    for (size_t i = 0; i < pstrlen(flags); i++) {
        if (pstrget(flags, i) != 'u')
            return PSTRING_EINVAL;
        props |= PSTREXPR_UTF8;
    }

    lit_t *lit = allocate(alloc, sizeof(*lit));
    char *needle = allocate(alloc, pstrlen(pattern) + 1);
    if (!lit || !needle) {
        deallocate(alloc, lit, sizeof(*lit));
        deallocate(alloc, needle, pstrlen(pattern) + 1);
        return PSTRING_ENOMEM;
    }

    size_t length = 0;
    for (size_t i = 0; i < pstrlen(pattern); i++) {
        char c = pstrget(pattern, i);
        if (c == '%' && i + 1 < pstrlen(pattern))
            c = pstrget(pattern, ++i);
        needle[length++] = c;
    }

    lit->needle = needle;
    lit->length = length;
    info->groups = 0;
    info->props = props;
    *prog = lit;
    return PSTRING_OK;
}

static void lit_destroy(void *prog, allocator_t *alloc, void *user) {
    (void)user;
    lit_t *lit = prog;
    deallocate(alloc, lit->needle, lit->length + 1 + 0 * sizeof(*lit));
    deallocate(alloc, lit, sizeof(*lit));
}

static int lit_exec(
    void *prog,
    const char *subject,
    size_t length,
    size_t start,
    pstrexpr_span_t *spans,
    size_t nspans,
    void *user
) {
    (void)user;
    (void)nspans;
    lit_t *lit = prog;

    if (lit->length == 0) {
        spans[0].start = spans[0].end = start;
        return PSTRING_TRUE;
    }

    pstring_t haystack, needle;
    pstrwrapb(
        &haystack, (char *)subject + start, length - start, length - start
    );
    pstrwrapb(&needle, lit->needle, lit->length, lit->length);

    char *found = pstrstr(&haystack, &needle);
    if (!found)
        return PSTRING_FALSE;

    spans[0].start = (size_t)(found - subject);
    spans[0].end = spans[0].start + lit->length;
    return PSTRING_TRUE;
}

static int litq_quote(pstring_t *out, const pstring_t *text, void *user) {
    (void)user;
    int rc = pstrcats(out, "<", 1);
    if (!rc)
        rc = pstrcat(out, text);
    if (!rc)
        rc = pstrcats(out, ">", 1);
    return rc;
}

/* Matches everything. Marked as a regex flavour, to test the "re" alias. */
static int any_exec(
    void *prog,
    const char *subject,
    size_t length,
    size_t start,
    pstrexpr_span_t *spans,
    size_t nspans,
    void *user
) {
    (void)prog;
    (void)subject;
    (void)length;
    (void)nspans;
    (void)user;
    spans[0].start = spans[0].end = start;
    return PSTRING_TRUE;
}

static const struct pstrexpr_vt lit_vt = {
    "lit", 0, '%', lit_compile, lit_destroy, lit_exec, NULL, NULL,
};

static const struct pstrexpr_vt litq_vt = {
    "litq", 0, 0, lit_compile, lit_destroy, lit_exec, NULL, litq_quote,
};

static const struct pstrexpr_vt any_vt = {
    "any", PSTREXPR_REGEX, 0, lit_compile, lit_destroy, any_exec, NULL, NULL,
};

static int initialized;
static int generic_missing;
static int generic_first;

static void init(void) {
    if (initialized)
        return;
    initialized = 1;

    pstrexpr_register("lit", &lit_vt, NULL);
    pstrexpr_register("litq", &litq_vt, NULL);

    /* only engines marked as regex flavours get the generic prefix */
    generic_missing = pstrmatch("/x/", PSTR("x"), NULL) == PSTRING_ENOENT
        && pstrmatch("re/x/", PSTR("x"), NULL) == PSTRING_ENOENT;

    /* ... and the first one to register keeps it */
    pstrexpr_register("any", &any_vt, NULL);
    generic_first = pstrmatch("re/zzz/", PSTR("abc"), NULL) == PSTRING_TRUE
        && pstrmatch("/zzz/", PSTR("abc"), NULL) == PSTRING_TRUE;

#ifdef TEST_POSIX
    pstrexpr_register_posix(NULL);
#endif
#ifdef PSTRING_TEST_PCRE2
    pstrexpr_register_pcre2(NULL);
#endif
#ifdef PSTRING_TEST_ONIG
    pstrexpr_register_onig(NULL);
#endif
}

static void own(pstring_t *str, const char *text) {
    *str = (pstring_t) { 0 };
    pstrnew(str, text, 0, NULL);
}

static int part_is(const pstrarray_t *array, size_t i, const char *text) {
    return i < array->length && pstrequals(&array->items[i], text, 0);
}

/* Tests */

int test_pattern_syntax(int seed, int rep) {
    init();
    pf_assert_true(generic_missing);

    pf_assert(pstrmatch("lit/abc/", PSTR("xxabcxx"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit/abd/", PSTR("xxabcxx"), NULL) == PSTRING_FALSE);

    /* delimiters */
    pf_assert(pstrmatch("lit:abc:", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit|abc|", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit#abc#", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit(abc)", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit[abc]", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit{abc}", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit<abc>", PSTR("abc"), NULL) == PSTRING_TRUE);

    /* brackets nest, other delimiters need to be escaped */
    pf_assert(pstrmatch("lit(a(b)c)", PSTR("xa(b)cx"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit/a%/b/", PSTR("a/b"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit(a%)b)", PSTR("a)b"), NULL) == PSTRING_TRUE);

    /* flags */
    pf_assert(pstrmatch("lit/abc/u", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("lit/abc/q", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit/a/b/", PSTR("a"), NULL) == PSTRING_EINVAL);

    /* malformed patterns */
    pf_assert(pstrmatch("lit/abc", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit/abc%/", PSTR("abc/"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit/abc%", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit(a(b)", PSTR("ab"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit%abc%", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit\\abc\\", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit abc ", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("nope/abc/", PSTR("abc"), NULL) == PSTRING_ENOENT);
    pf_assert(
        pstrmatch("0123456789012345678901234567890123/a/", PSTR("a"), NULL)
        == PSTRING_ENOENT
    );

    /* invalid arguments */
    pf_assert(pstrmatch(NULL, PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("", PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit/a/", NULL, NULL) == PSTRING_EINVAL);
    return 0;
}

int test_pattern_register(int seed, int rep) {
    init();

    pf_assert(pstrexpr_register(NULL, &lit_vt, NULL) == PSTRING_EINVAL);
    pf_assert(pstrexpr_register("", &lit_vt, NULL) == PSTRING_EINVAL);
    pf_assert(pstrexpr_register("a-b", &lit_vt, NULL) == PSTRING_EINVAL);
    pf_assert(pstrexpr_register("x/y", &lit_vt, NULL) == PSTRING_EINVAL);
    pf_assert(pstrexpr_register("fresh", NULL, NULL) == PSTRING_EINVAL);
    pf_assert(
        pstrexpr_register("0123456789012345678901234567890123", &lit_vt, NULL)
        == PSTRING_EINVAL
    );
    pf_assert(pstrexpr_register("lit", &lit_vt, NULL) == PSTRING_EEXIST);

    struct pstrexpr_vt broken = lit_vt;
    broken.exec = NULL;
    pf_assert(pstrexpr_register("broken", &broken, NULL) == PSTRING_EINVAL);

    broken = lit_vt;
    broken.escape = 'a';
    pf_assert(pstrexpr_register("broken", &broken, NULL) == PSTRING_EINVAL);

    pf_assert(pstrexpr_alias("who", "missing") == PSTRING_ENOENT);
    pf_assert(pstrexpr_alias("lit", "litq") == PSTRING_EEXIST);
    pf_assert(pstrexpr_alias("a-b", "lit") == PSTRING_EINVAL);
    return 0;
}

int test_pattern_alias(int seed, int rep) {
    init();
    pf_assert_true(generic_first);

    /* the same pattern changes meaning when "re" is rebound, which also
       shows that the cache doesn't hold on to stale patterns */
    pf_assert(pstrmatch("re/zzz/", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrexpr_alias("re", "lit") == PSTRING_OK);
    pf_assert(pstrmatch("re/zzz/", PSTR("abc"), NULL) == PSTRING_FALSE);
    pf_assert(pstrmatch("/b/", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrexpr_alias("re", "any") == PSTRING_OK);
    pf_assert(pstrmatch("re/zzz/", PSTR("abc"), NULL) == PSTRING_TRUE);

    /* custom aliases */
    pf_assert(pstrexpr_alias("literal", "lit") == PSTRING_OK);
    pf_assert(pstrmatch("literal/b/", PSTR("abc"), NULL) == PSTRING_TRUE);
    pf_assert(pstrexpr_alias("literal", "any") == PSTRING_OK);
    pf_assert(pstrmatch("literal/zzz/", PSTR("abc"), NULL) == PSTRING_TRUE);
    return 0;
}

int test_pattern_compile(int seed, int rep) {
    init();

    counting_t counting = { { counting_fn }, 0 };
    pstrexpr_t *expr = (pstrexpr_t *)1;

    pf_assert(pstrexpr_compile(&expr, "nope/a/", NULL) == PSTRING_ENOENT);
    pf_assert_null(expr);
    pf_assert(pstrexpr_compile(&expr, "lit/a/z", &counting.base) < 0);
    pf_assert_null(expr);
    pf_assert(pstrexpr_compile(NULL, "lit/a/", NULL) == PSTRING_EINVAL);
    pf_assert(counting.live == 0);

    pf_assert_ok(pstrexpr_compile(&expr, "lit/abc/", &counting.base));
    pf_assert_not_null(expr);
    pf_assert(counting.live > 0);
    pf_assert(pstrexpr_groups(expr) == 0);

    pstrarray_t caps = { 0 };
    pf_assert(pstrexpr_match(expr, PSTR("--abc--"), &caps) == PSTRING_TRUE);
    pf_assert(caps.length == 1);
    pf_assert(part_is(&caps, 0, "abc"));
    pf_assert(pstrexpr_match(expr, PSTR("--abd--"), &caps) == PSTRING_FALSE);
    pf_assert(caps.length == 0);
    pstrarray_free(&caps);

    pf_assert(pstrexpr_match(NULL, PSTR("abc"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrexpr_match(expr, NULL, NULL) == PSTRING_EINVAL);

    pstrexpr_free(expr);
    pf_assert(counting.live == 0);

    pstrexpr_free(NULL);
    return 0;
}

int test_pattern_captures(int seed, int rep) {
    init();

    /* captures are slices of the source */
    pstring_t src = PSTRWRAP("xxabcxx");
    pstrarray_t caps = { 0 };
    pf_assert(pstrmatch("lit/abc/", &src, &caps) == PSTRING_TRUE);
    pf_assert(pstrbuf(&caps.items[0]) == pstrbuf(&src) + 2);
    pf_assert(pstrexpr_isset(&caps.items[0]));
    pf_assert(!pstrexpr_isset(NULL));

    /* arrays are reusable */
    pf_assert(pstrmatch("lit/xx/", &src, &caps) == PSTRING_TRUE);
    pf_assert(caps.length == 1);
    pstrarray_free(&caps);

    /* a user supplied buffer is never reallocated */
    pstring_t buffer[1];
    pstrarray_t fixed = { .capacity = 1, .items = buffer };
    pf_assert(pstrmatch("lit/abc/", &src, &fixed) == PSTRING_TRUE);
    pf_assert(fixed.length == 1);

    pstrarray_t none = { .capacity = 0, .items = buffer };
    pf_assert(pstrmatch("lit/abc/", &src, &none) == PSTRING_ERANGE);

    /* a custom allocator is respected */
    counting_t counting = { { counting_fn }, 0 };
    pstrarray_t custom = { .allocator = &counting.base };
    pf_assert(pstrmatch("lit/abc/", &src, &custom) == PSTRING_TRUE);
    pf_assert(counting.live == 1);
    pstrarray_free(&custom);
    pf_assert(counting.live == 0);
    return 0;
}

int test_pattern_split(int seed, int rep) {
    init();
    pstrarray_t parts = { 0 };

    pf_assert(pstrsplitp("lit/,/", PSTR("a,b,,c"), &parts, 0) == 4);
    pf_assert(part_is(&parts, 0, "a"));
    pf_assert(part_is(&parts, 1, "b"));
    pf_assert(part_is(&parts, 2, ""));
    pf_assert(part_is(&parts, 3, "c"));

    pf_assert(pstrsplitp("lit/,/", PSTR("a,b,,c"), &parts, 1) == 2);
    pf_assert(part_is(&parts, 0, "a"));
    pf_assert(part_is(&parts, 1, "b,,c"));

    pf_assert(pstrsplitp("lit/,/", PSTR("a,b,,c"), &parts, 2) == 3);
    pf_assert(part_is(&parts, 2, ",c"));

    /* no match, empty source, matches at the edges */
    pf_assert(pstrsplitp("lit/;/", PSTR("a,b"), &parts, 0) == 1);
    pf_assert(part_is(&parts, 0, "a,b"));
    pf_assert(pstrsplitp("lit/;/", PSTR(""), &parts, 0) == 1);
    pf_assert(part_is(&parts, 0, ""));
    pf_assert(pstrsplitp("lit/,/", PSTR(",a,"), &parts, 0) == 3);
    pf_assert(part_is(&parts, 0, ""));
    pf_assert(part_is(&parts, 1, "a"));
    pf_assert(part_is(&parts, 2, ""));

    /* an empty pattern splits between characters, like in JavaScript */
    pf_assert(pstrsplitp("lit//", PSTR("abc"), &parts, 0) == 3);
    pf_assert(part_is(&parts, 0, "a"));
    pf_assert(part_is(&parts, 1, "b"));
    pf_assert(part_is(&parts, 2, "c"));

    pf_assert(pstrsplitp("lit/,/", NULL, &parts, 0) == PSTRING_EINVAL);
    pf_assert(pstrsplitp("lit/,/", PSTR("a"), NULL, 0) == PSTRING_EINVAL);
    pf_assert(pstrsplitp("nope/,/", PSTR("a"), &parts, 0) == PSTRING_ENOENT);

    pstrarray_free(&parts);
    return 0;
}

int test_pattern_repl(int seed, int rep) {
    init();
    pstring_t str;

    own(&str, "banana");
    pf_assert(pstrreplp("lit/a/", "<$0>", &str, 0) == 3);
    pf_assert(pstrequals(&str, "b<a>n<a>n<a>", 0));
    pstrfree(&str);

    own(&str, "banana");
    pf_assert(pstrreplp("lit/a/", "<${0}>", &str, 2) == 2);
    pf_assert(pstrequals(&str, "b<a>n<a>na", 0));
    pstrfree(&str);

    own(&str, "banana");
    pf_assert(pstrreplp("lit/an/", "", &str, 0) == 2);
    pf_assert(pstrequals(&str, "ba", 0));
    pstrfree(&str);

    own(&str, "a.b");
    pf_assert(pstrreplp("lit/./", "$$", &str, 0) == 1);
    pf_assert(pstrequals(&str, "a$b", 0));
    pstrfree(&str);

    /* no match leaves the string alone */
    own(&str, "banana");
    pf_assert(pstrreplp("lit/x/", "y", &str, 0) == 0);
    pf_assert(pstrequals(&str, "banana", 0));
    pstrfree(&str);

    /* the result may be much longer than the source (and leave SSO) */
    own(&str, "a-a-a-a");
    pf_assert(pstrreplp("lit/a/", "0123456789", &str, 0) == 4);
    pf_assert(
        pstrequals(&str, "0123456789-0123456789-0123456789-0123456789", 0)
    );
    pstrfree(&str);

    /* empty matches */
    own(&str, "abc");
    pf_assert(pstrreplp("lit//", "-", &str, 0) == 4);
    pf_assert(pstrequals(&str, "-a-b-c-", 0));
    pstrfree(&str);

    /* invalid templates are caught before the string is modified */
    own(&str, "banana");
    pf_assert(pstrreplp("lit/a/", "$", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("lit/a/", "x$y", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("lit/a/", "${", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("lit/a/", "${}", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("lit/a/", "${1x}", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("lit/a/", "$1", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("lit/a/", "${name}", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("lit/a/", NULL, &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrequals(&str, "banana", 0));
    pstrfree(&str);

    /* slices can't be modified */
    pstring_t slice = PSTRWRAP("banana");
    pf_assert(pstrreplp("lit/a/", "x", &slice, 0) == PSTRING_EINVAL);
    pf_assert(pstrequals(&slice, "banana", 0));
    return 0;
}

struct iter_state {
    size_t starts[8];
    size_t count;
    size_t stop_at;
    int abort;
};

static int iter_record(const pstrarray_t *groups, size_t index, void *user) {
    struct iter_state *state = user;
    if (index != state->count || groups->length != 1)
        return PSTRING_EINVAL;

    state->starts[state->count++] = 0;
    return 0;
}

static int iter_control(const pstrarray_t *groups, size_t index, void *user) {
    (void)groups;
    struct iter_state *state = user;
    state->count = index + 1;

    if (state->abort && index == 1)
        return PSTRING_EIO;
    return state->stop_at && index + 1 >= state->stop_at;
}

static int count_matches(const char *pattern, const char *text) {
    pstring_t src;
    pstrwrap(&src, (char *)text, 0, 0);
    return pstriterp(pattern, &src, iter_control, &(struct iter_state) { 0 });
}

int test_pattern_iter(int seed, int rep) {
    init();
    struct iter_state state = { 0 };

    pf_assert(pstriterp("lit/a/", PSTR("banana"), iter_record, &state) == 3);
    pf_assert(state.count == 3);

    state = (struct iter_state) { 0 };
    pf_assert(pstriterp("lit/a/", PSTR("banana"), iter_control, &state) == 3);

    state = (struct iter_state) { .stop_at = 2 };
    pf_assert(pstriterp("lit/a/", PSTR("banana"), iter_control, &state) == 2);
    pf_assert(state.count == 2);

    state = (struct iter_state) { .abort = 1 };
    pf_assert(
        pstriterp("lit/a/", PSTR("banana"), iter_control, &state) == PSTRING_EIO
    );

    pf_assert(pstriterp("lit/x/", PSTR("banana"), iter_record, &state) == 0);
    pf_assert(
        pstriterp("lit/a/", PSTR("banana"), NULL, &state) == PSTRING_EINVAL
    );

    /* empty matches are found at every position, including the end */
    state = (struct iter_state) { 0 };
    pf_assert(pstriterp("lit//", PSTR("abc"), iter_control, &state) == 4);
    return 0;
}

int test_pattern_utf8_steps(int seed, int rep) {
    init();
    pstring_t str;

    /* "é!" is 3 bytes: with the `u` flag empty matches skip whole characters */
    own(&str, "\xC3\xA9!");
    pf_assert(pstrreplp("lit//u", "-", &str, 0) == 3);
    pf_assert(pstrequals(&str, "-\xC3\xA9-!-", 0));
    pstrfree(&str);

    own(&str, "\xC3\xA9!");
    pf_assert(pstrreplp("lit//", "-", &str, 0) == 4);
    pf_assert(pstrequals(&str, "-\xC3-\xA9-!-", 0));
    pstrfree(&str);
    return 0;
}

int test_pattern_escape(int seed, int rep) {
    init();
    pstring_t out = { 0 };

    pf_assert_ok(pstrexpr_escape(&out, "lit", PSTR("a.b*c d")));
    pf_assert(pstrequals(&out, "a%.b%*c% d", 0));
    pstrfree(&out);
    out = (pstring_t) { 0 }; /* pstrfree keeps SSO contents */

    /* the quote hook of the engine takes precedence */
    pf_assert_ok(pstrexpr_escape(&out, "litq", PSTR("a.b")));
    pf_assert(pstrequals(&out, "<a.b>", 0));
    pstrfree(&out);
    out = (pstring_t) { 0 }; /* pstrfree keeps SSO contents */

    /* appends, and leaves alphanumerics and non-ASCII alone */
    pf_assert_ok(pstrcats(&out, "x", 0));
    pf_assert_ok(pstrexpr_escape(&out, "lit", PSTR("_a1\xC3\xA9")));
    pf_assert(pstrequals(&out, "x_a1\xC3\xA9", 0));
    pstrfree(&out);
    out = (pstring_t) { 0 }; /* pstrfree keeps SSO contents */

    /* escaped text always matches itself literally */
    pstring_t pattern = { 0 };
    pf_assert_ok(pstrcats(&pattern, "lit/", 0));
    pf_assert_ok(pstrexpr_escape(&pattern, "lit", PSTR("50% off/on")));
    pf_assert_ok(pstrcats(&pattern, "/", 0));
    pstrterm(&pattern, NULL);
    pf_assert(
        pstrmatch(pstrbuf(&pattern), PSTR("get 50% off/on now"), NULL)
        == PSTRING_TRUE
    );
    pstrfree(&pattern);

    pf_assert(pstrexpr_escape(&out, "nope", PSTR("a")) == PSTRING_ENOENT);
    pf_assert(pstrexpr_escape(NULL, "lit", PSTR("a")) == PSTRING_EINVAL);
    pf_assert(pstrexpr_escape(&out, "lit", NULL) == PSTRING_EINVAL);
    return 0;
}

int test_pattern_quote(int seed, int rep) {
    init();
    pstring_t pat = { 0 };

    /* the delimiter can't be fooled by the text, whatever it contains */
    const char *nasty[] = {
        "a/b", "a/|#b", "plain", "", "/|#!~@:;,=&%+-^$.*?\"'`"
    };
    for (size_t i = 0; i < sizeof(nasty) / sizeof(*nasty); i++) {
        pstring_t text;
        pstrwrap(&text, (char *)nasty[i], 0, 0);
        if (i == 3)
            text = (pstring_t) { 0 };

        pat = (pstring_t) { 0 };
        pf_assert_ok(pstrexpr_quote(&pat, "lit", &text, NULL));
        pstrterm(&pat, NULL);

        pstring_t hay = { 0 };
        pstrcats(&hay, "<<", 0);
        pstrcat(&hay, &text);
        pstrcats(&hay, ">>", 0);

        pstrarray_t caps = { 0 };
        pf_assert(pstrmatch(pstrbuf(&pat), &hay, &caps) == PSTRING_TRUE);
        pf_assert(pstrequal(&caps.items[0], &text));
        pstrarray_free(&caps);
        pstrfree(&hay);
        pstrfree(&pat);
    }

    /* flags are appended, and failures leave `out` alone */
    pat = (pstring_t) { 0 };
    pstrcats(&pat, "keep", 0);
    pf_assert_ok(pstrexpr_quote(&pat, "lit", PSTR("a.b"), "u"));
    pf_assert(pstrprefix(&pat, "keep", 0));
    pf_assert(pstrsuffix(&pat, "u", 0));
    pstrfree(&pat);

    pat = (pstring_t) { 0 };
    pstrcats(&pat, "keep", 0);
    pf_assert(pstrexpr_quote(&pat, "nope", PSTR("a"), NULL) == PSTRING_ENOENT);
    pf_assert(pstrexpr_quote(&pat, "lit", NULL, NULL) == PSTRING_EINVAL);
    pf_assert(pstrexpr_quote(NULL, "lit", PSTR("a"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrequals(&pat, "keep", 0));
    pstrfree(&pat);
    return 0;
}

int test_pattern_cache(int seed, int rep) {
    init();

    /* more patterns than the cache can hold, visited repeatedly */
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < PSTREXPR_CACHE_SIZE * 3; i++) {
            char pattern[32], text[32];
            snprintf(pattern, sizeof(pattern), "lit/k%d;/", i);
            snprintf(text, sizeof(text), "xk%d;y", i);

            pstring_t src;
            pstrwrap(&src, text, 0, 0);
            pf_assert(pstrmatch(pattern, &src, NULL) == PSTRING_TRUE);

            snprintf(text, sizeof(text), "xk%d;y", i + 1);
            pstrwrap(&src, text, 0, 0);
            pf_assert(pstrmatch(pattern, &src, NULL) == PSTRING_FALSE);
        }
    }

    pstrexpr_cache_clear();
    pstrexpr_cache_clear();
    pf_assert(pstrmatch("lit/a/", PSTR("a"), NULL) == PSTRING_TRUE);

    /* failures aren't cached, and don't poison later calls */
    pf_assert(pstrmatch("lit/a/q", PSTR("a"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("lit/a/q", PSTR("a"), NULL) == PSTRING_EINVAL);
    return 0;
}

#ifdef TEST_THREADS

static int cache_worker(void *arg) {
    int failures = 0;
    int offset = (int)(intptr_t)arg;

    for (int i = 0; i < 3000; i++) {
        int n = (i * 7 + offset) % 80;
        char pattern[32], text[32];
        snprintf(pattern, sizeof(pattern), "lit/k%d;/", n);
        snprintf(text, sizeof(text), "xk%d;y", n);

        pstring_t src;
        pstrwrap(&src, text, 0, 0);
        failures += pstrmatch(pattern, &src, NULL) != PSTRING_TRUE;

        snprintf(text, sizeof(text), "xk%d;y", n + 1);
        pstrwrap(&src, text, 0, 0);
        failures += pstrmatch(pattern, &src, NULL) != PSTRING_FALSE;

        if (i % 500 == 0)
            pstrexpr_cache_clear();
    }

    return failures;
}

int test_pattern_threads(int seed, int rep) {
    init();

    thrd_t threads[4];
    for (int i = 0; i < 4; i++)
        pf_assert(
            thrd_create(&threads[i], cache_worker, (void *)(intptr_t)(i * 13))
            == thrd_success
        );

    int failures = 0;
    for (int i = 0; i < 4; i++) {
        int result = 0;
        thrd_join(threads[i], &result);
        failures += result;
    }

    pf_assert(failures == 0);
    return 0;
}

#endif

#ifdef TEST_POSIX

int test_pattern_posix(int seed, int rep) {
    init();

    pstring_t text = PSTRWRAP("mail bob@site.org now");
    pstrarray_t caps = { 0 };

    pf_assert(
        pstrmatch("posix/([a-z]+)@([a-z]+)\\.(com|org)/", &text, &caps)
        == PSTRING_TRUE
    );
    pf_assert(caps.length == 4);
    pf_assert(part_is(&caps, 0, "bob@site.org"));
    pf_assert(part_is(&caps, 1, "bob"));
    pf_assert(part_is(&caps, 2, "site"));
    pf_assert(part_is(&caps, 3, "org"));
    pf_assert(pstrbuf(&caps.items[1]) == pstrbuf(&text) + 5);

    pf_assert(pstrmatch("posix/[0-9]+/", &text, &caps) == PSTRING_FALSE);
    pf_assert(caps.length == 0);

    /* unset groups differ from empty ones */
    pf_assert(pstrmatch("posix/(a)|(b)/", PSTR("b"), &caps) == PSTRING_TRUE);
    pf_assert(caps.length == 3);
    pf_assert(!pstrexpr_isset(&caps.items[1]));
    pf_assert(pstrexpr_isset(&caps.items[2]));

    pf_assert(pstrmatch("posix/(x*)b/", PSTR("b"), &caps) == PSTRING_TRUE);
    pf_assert(pstrexpr_isset(&caps.items[1]));
    pf_assert(pstrlen(&caps.items[1]) == 0);
    pstrarray_free(&caps);

    /* flags and errors */
    pf_assert(
        pstrmatch("posix/HELLO/i", PSTR("say hello"), NULL) == PSTRING_TRUE
    );
    pf_assert(
        pstrmatch("posix/HELLO/", PSTR("say hello"), NULL) == PSTRING_FALSE
    );
    pf_assert(pstrmatch("posix/a.b/n", PSTR("a\nb"), NULL) == PSTRING_FALSE);
    pf_assert(pstrmatch("posix/a.b/", PSTR("a\nb"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("posix/a+/b", PSTR("a+"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("posix/(/", PSTR("a"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("posix/a/z", PSTR("a"), NULL) == PSTRING_EINVAL);

    /* the generic prefix goes to the first regex flavour registered */
    pf_assert(pstrexpr_alias("re", "posix") == PSTRING_OK);
    pf_assert(pstrmatch("/a+/", PSTR("caat"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("re/a{2}/", PSTR("caat"), NULL) == PSTRING_TRUE);

    /* an escaped delimiter reaches the engine as an escaped character */
    pf_assert(pstrmatch("posix/a\\/b/", PSTR("a/b"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("posix{a{2}b}", PSTR("aab"), NULL) == PSTRING_TRUE);
    pf_assert(pstrexpr_alias("re", "any") == PSTRING_OK);
    return 0;
}

int test_pattern_posix_all(int seed, int rep) {
    init();
    pstring_t str;

    /* replacement with groups */
    own(&str, "bob@site, amy@home");
    pf_assert(pstrreplp("posix/([a-z]+)@([a-z]+)/", "$2 at $1", &str, 0) == 2);
    pf_assert(pstrequals(&str, "site at bob, home at amy", 0));
    pstrfree(&str);

    own(&str, "bob@site");
    pf_assert(pstrreplp("posix/(a)|(b)/", "[$1|$2]", &str, 1) == 1);
    pf_assert(pstrequals(&str, "[|b]ob@site", 0));
    pstrfree(&str);

    /* named groups are not part of POSIX */
    own(&str, "bob");
    pf_assert(pstrreplp("posix/(b)/", "${name}", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrreplp("posix/(b)/", "$2", &str, 0) == PSTRING_EINVAL);
    pf_assert(pstrequals(&str, "bob", 0));
    pstrfree(&str);

    /* empty matches follow the Python 3.7+ rules */
    own(&str, "abxd");
    pf_assert(pstrreplp("posix/x*/", "-", &str, 0) == 5);
    pf_assert(pstrequals(&str, "-a-b--d-", 0));
    pstrfree(&str);

    /* continuing a search must not re-anchor at the new start */
    pf_assert(count_matches("posix/^a/", "aaa") == 1);
    pf_assert(count_matches("posix/a/", "aaa") == 3);
    pf_assert(count_matches("posix/b*/", "abba") == 4);

    /* splitting */
    pstrarray_t parts = { 0 };
    pf_assert(pstrsplitp("posix/[,;] */", PSTR("a, b;c,d"), &parts, 0) == 4);
    pf_assert(part_is(&parts, 0, "a"));
    pf_assert(part_is(&parts, 1, "b"));
    pf_assert(part_is(&parts, 2, "c"));
    pf_assert(part_is(&parts, 3, "d"));

    pf_assert(pstrsplitp("posix/x*/", PSTR("axb"), &parts, 0) == 2);
    pf_assert(part_is(&parts, 0, "a"));
    pf_assert(part_is(&parts, 1, "b"));
    pstrarray_free(&parts);

    /* more groups than the stack buffers hold */
    char pattern[512] = "posix/";
    char text[128] = { 0 };
    for (int i = 0; i < 40; i++) {
        strcat(pattern, "(a)");
        strcat(text, "a");
    }
    strcat(pattern, "/");

    pstrarray_t caps = { 0 };
    pstring_t src;
    pstrwrap(&src, text, 0, 0);
    pf_assert(pstrmatch(pattern, &src, &caps) == PSTRING_TRUE);
    pf_assert(caps.length == 41);
    pf_assert(part_is(&caps, 40, "a"));
    pstrarray_free(&caps);

    /* escaping for POSIX */
    pstring_t escaped = { 0 };
    pstrcats(&escaped, "posix/^", 0);
    pf_assert_ok(pstrexpr_escape(&escaped, "posix", PSTR("1+1=2 (a|b)?")));
    pstrcats(&escaped, "$/", 0);
    pstrterm(&escaped, NULL);
    pf_assert(
        pstrmatch(pstrbuf(&escaped), PSTR("1+1=2 (a|b)?"), NULL) == PSTRING_TRUE
    );
    pf_assert(
        pstrmatch(pstrbuf(&escaped), PSTR("1+1=2 (a|b)"), NULL) == PSTRING_FALSE
    );
    pstrfree(&escaped);
    return 0;
}

#endif

#ifdef PSTRING_TEST_PCRE2

int test_pattern_pcre2(int seed, int rep) {
    init();
    pstrarray_t caps = { 0 };
    pstring_t str;

    pstring_t date = PSTRWRAP("on 2026-03-14 or 2027-01-02");
    pf_assert(
        pstrmatch("pcre/(?<y>\\d{4})-(?<m>\\d\\d)/", &date, &caps)
        == PSTRING_TRUE
    );
    pf_assert(caps.length == 3);
    pf_assert(part_is(&caps, 1, "2026"));
    pf_assert(part_is(&caps, 2, "03"));
    pstrarray_free(&caps);

    /* named group references */
    own(&str, "on 2026-03-14 or 2027-01-02");
    pf_assert(
        pstrreplp("pcre/(?<y>\\d{4})-(?<m>\\d\\d)/", "${m}/${y}", &str, 0) == 2
    );
    pf_assert(pstrequals(&str, "on 03/2026-14 or 01/2027-02", 0));
    pf_assert(
        pstrreplp("pcre/(?<y>\\d{4})/", "${nope}", &str, 0) == PSTRING_ENOENT
    );
    pstrfree(&str);

    /* look-behinds see the text before the search start */
    pf_assert(count_matches("pcre/(?<=a)b/", "abab") == 2);
    pf_assert(count_matches("pcre/(?<=a)b/", "bbab") == 1);
    pf_assert(count_matches("pcre/^a/", "aaa") == 1);
    pf_assert(count_matches("pcre/\\bx/", "xx x") == 2);

    /* flags */
    pf_assert(
        pstrmatch("pcre/HELLO/i", PSTR("say hello"), NULL) == PSTRING_TRUE
    );
    pf_assert(pstrmatch("pcre/a.b/", PSTR("a\nb"), NULL) == PSTRING_FALSE);
    pf_assert(pstrmatch("pcre/a.b/s", PSTR("a\nb"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("pcre/^b/m", PSTR("a\nb"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("pcre/^b/", PSTR("a\nb"), NULL) == PSTRING_FALSE);
    pf_assert(pstrmatch("pcre/a b/x", PSTR("ab"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("pcre/^.$/u", PSTR("\xC3\xA9"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("pcre/^.$/", PSTR("\xC3\xA9"), NULL) == PSTRING_FALSE);
    pf_assert(pstrmatch("pcre/(/", PSTR("a"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("pcre/a/z", PSTR("a"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("pcre/a/u", PSTR("\xFF"), NULL) < 0);

    /* empty matches */
    own(&str, "abxd");
    pf_assert(pstrreplp("pcre/x*/", "-", &str, 0) == 5);
    pf_assert(pstrequals(&str, "-a-b--d-", 0));
    pstrfree(&str);

    own(&str, "\xC3\xA9!");
    pf_assert(pstrreplp("pcre//u", "-", &str, 0) == 3);
    pf_assert(pstrequals(&str, "-\xC3\xA9-!-", 0));
    pstrfree(&str);

    /* escaping, including the brackets which need no delimiter escapes */
    pf_assert(
        pstrmatch("pcre(a(?:b|c){2}d)", PSTR("abcd"), NULL) == PSTRING_TRUE
    );

    pstring_t pattern = { 0 };
    pstrcats(&pattern, "pcre/^", 0);
    pf_assert_ok(pstrexpr_escape(&pattern, "pcre", PSTR("a.b|c\\d/e")));
    pstrcats(&pattern, "$/", 0);
    pstrterm(&pattern, NULL);
    pf_assert(
        pstrmatch(pstrbuf(&pattern), PSTR("a.b|c\\d/e"), NULL) == PSTRING_TRUE
    );
    pf_assert(
        pstrmatch(pstrbuf(&pattern), PSTR("axb|c\\d/e"), NULL) == PSTRING_FALSE
    );
    pstrfree(&pattern);
    return 0;
}

#endif

#ifdef PSTRING_TEST_ONIG

int test_pattern_onig(int seed, int rep) {
    init();
    pstrarray_t caps = { 0 };
    pstring_t str;

    pf_assert(
        pstrmatch("onig/(?<k>\\w+)=(\\d+)/", PSTR("x a=12 y"), &caps)
        == PSTRING_TRUE
    );
    pf_assert(caps.length == 3);
    pf_assert(part_is(&caps, 0, "a=12"));
    pf_assert(part_is(&caps, 1, "a"));
    pf_assert(part_is(&caps, 2, "12"));
    pstrarray_free(&caps);

    own(&str, "a=1, b=2");
    pf_assert(
        pstrreplp("onig/(?<k>\\w+)=(?<v>\\d+)/", "${v}=${k}", &str, 0) == 2
    );
    pf_assert(pstrequals(&str, "1=a, 2=b", 0));
    pf_assert(
        pstrreplp("onig/(?<k>\\w)/", "${nope}", &str, 0) == PSTRING_ENOENT
    );
    pstrfree(&str);

    /* UTF-8 aware */
    pf_assert(pstrmatch("onig/^.$/", PSTR("\xC3\xA9"), NULL) == PSTRING_TRUE);
    pf_assert(
        pstrmatch("onig/HELLO/i", PSTR("say hello"), NULL) == PSTRING_TRUE
    );
    pf_assert(pstrmatch("onig/a.b/", PSTR("a\nb"), NULL) == PSTRING_FALSE);
    pf_assert(pstrmatch("onig/a.b/s", PSTR("a\nb"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("onig/^b/m", PSTR("a\nb"), NULL) == PSTRING_TRUE);
    pf_assert(pstrmatch("onig/^b/", PSTR("a\nb"), NULL) == PSTRING_FALSE);
    pf_assert(pstrmatch("onig/(/", PSTR("a"), NULL) == PSTRING_EINVAL);
    pf_assert(pstrmatch("onig/a/z", PSTR("a"), NULL) == PSTRING_EINVAL);

    own(&str, "\xC3\xA9!");
    pf_assert(pstrreplp("onig//", "-", &str, 0) == 3);
    pf_assert(pstrequals(&str, "-\xC3\xA9-!-", 0));
    pstrfree(&str);

    pf_assert(count_matches("onig/(?<=a)b/", "abab") == 2);
    pf_assert(count_matches("onig/^a/", "aaa") == 1);
    return 0;
}

#endif

const pf_test_t suite_pattern[] = {
    { test_pattern_syntax, "/pstring/pattern/syntax" },
    { test_pattern_register, "/pstring/pattern/register" },
    { test_pattern_alias, "/pstring/pattern/alias" },
    { test_pattern_compile, "/pstring/pattern/compile" },
    { test_pattern_captures, "/pstring/pattern/captures" },
    { test_pattern_split, "/pstring/pattern/split" },
    { test_pattern_repl, "/pstring/pattern/repl" },
    { test_pattern_iter, "/pstring/pattern/iter" },
    { test_pattern_utf8_steps, "/pstring/pattern/utf8_steps" },
    { test_pattern_escape, "/pstring/pattern/escape" },
    { test_pattern_quote, "/pstring/pattern/quote" },
    { test_pattern_cache, "/pstring/pattern/cache" },
#ifdef TEST_THREADS
    { test_pattern_threads, "/pstring/pattern/threads" },
#endif
#ifdef TEST_POSIX
    { test_pattern_posix, "/pstring/pattern/posix" },
    { test_pattern_posix_all, "/pstring/pattern/posix_all" },
#endif
#ifdef PSTRING_TEST_PCRE2
    { test_pattern_pcre2, "/pstring/pattern/pcre2" },
#endif
#ifdef PSTRING_TEST_ONIG
    { test_pattern_onig, "/pstring/pattern/onig" },
#endif
    { 0 },
};
