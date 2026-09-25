/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pf_assert.h>
#include <pf_test.h>

#include <pstring/glob.h>
#include <pstring/pstring.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #include <process.h>
    #define MKDIR(p) _mkdir(p)
    #define getpid _getpid
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define MKDIR(p) mkdir(p, 0755)
#endif

int test_glob_match_basic(int seed, int rep) {
    pf_assert_true(pstrglob_match("*.c", &PSTRWRAP("main.c"), 0));
    pf_assert_false(pstrglob_match("*.c", &PSTRWRAP("main.h"), 0));
    pf_assert_true(pstrglob_match("?ain.c", &PSTRWRAP("main.c"), 0));
    pf_assert_true(pstrglob_match("m*.c", &PSTRWRAP("main.c"), 0));
    pf_assert_true(pstrglob_match("m*n.c", &PSTRWRAP("main.c"), 0));
    pf_assert_true(pstrglob_match("*", &PSTRWRAP("anything"), 0));
    pf_assert_true(pstrglob_match("exact", &PSTRWRAP("exact"), 0));
    pf_assert_false(pstrglob_match("exact", &PSTRWRAP("exacts"), 0));
    return 0;
}

int test_glob_match_brackets(int seed, int rep) {
    pf_assert_true(pstrglob_match("[abc].c", &PSTRWRAP("a.c"), 0));
    pf_assert_false(pstrglob_match("[abc].c", &PSTRWRAP("d.c"), 0));
    pf_assert_true(pstrglob_match("[a-c].c", &PSTRWRAP("b.c"), 0));
    pf_assert_false(pstrglob_match("[a-c].c", &PSTRWRAP("d.c"), 0));
    pf_assert_true(pstrglob_match("[!abc].c", &PSTRWRAP("d.c"), 0));
    pf_assert_false(pstrglob_match("[!abc].c", &PSTRWRAP("a.c"), 0));
    pf_assert_true(pstrglob_match("[^abc].c", &PSTRWRAP("d.c"), 0));
    return 0;
}

int test_glob_match_period(int seed, int rep) {
    pf_assert_false(pstrglob_match("*", &PSTRWRAP(".hidden"), 0));
    pf_assert_true(pstrglob_match("*", &PSTRWRAP(".hidden"), PSTRGLOB_PERIOD));
    pf_assert_true(pstrglob_match(".*", &PSTRWRAP(".hidden"), 0));
    return 0;
}

int test_glob_match_escape(int seed, int rep) {
    pf_assert_true(pstrglob_match("a\\*b", &PSTRWRAP("a*b"), 0));
    pf_assert_false(pstrglob_match("a\\*b", &PSTRWRAP("ab"), 0));
    pf_assert_true(
        pstrglob_match("a\\*b", &PSTRWRAP("a\\*b"), PSTRGLOB_NOESCAPE)
    );
    return 0;
}

int test_glob_match_errors(int seed, int rep) {
    pf_assert(PSTRTHROW_EINVAL == pstrglob_match(NULL, &PSTRWRAP("x"), 0));
    pf_assert(PSTRTHROW_EINVAL == pstrglob_match("x", NULL, 0));
    return 0;
}

static void make_tree(const char *root) {
    char path[512];
    MKDIR(root);

    snprintf(path, sizeof(path), "%s/a.c", root);
    fclose(fopen(path, "w"));
    snprintf(path, sizeof(path), "%s/b.c", root);
    fclose(fopen(path, "w"));
    snprintf(path, sizeof(path), "%s/c.h", root);
    fclose(fopen(path, "w"));

    snprintf(path, sizeof(path), "%s/sub", root);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/sub/d.c", root);
    fclose(fopen(path, "w"));
}

static void remove_tree(const char *root) {
    char path[512];
    snprintf(path, sizeof(path), "%s/a.c", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/b.c", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/c.h", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/sub/d.c", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/sub", root);
    remove(path);
    remove(root);
}

static int array_contains(pstrarray_t *arr, const char *needle) {
    for (size_t i = 0; i < arr->length; i++) {
        if (pstrequals(&arr->items[i], needle, 0))
            return 1;
    }
    return 0;
}

static pstring_t wrap(const char *s) {
    pstring_t str;
    pstrwrap(&str, (char *)s, 0, 0);
    return str;
}

int test_glob_filesystem(int seed, int rep) {
    char root[64];
    snprintf(root, sizeof(root), "pstring-glob-test-%d", getpid());
    make_tree(root);

    char pattern[128];
    pstring_t pat;
    pstrarray_t dst = { 0 };

    snprintf(pattern, sizeof(pattern), "%s/*.c", root);
    pat = wrap(pattern);
    pf_assert_ok(pstrglob(&dst, &pat, 0, NULL));
    pf_assert(dst.length == 2);

    char expect[128];
    snprintf(expect, sizeof(expect), "%s/a.c", root);
    pf_assert_true(array_contains(&dst, expect));
    snprintf(expect, sizeof(expect), "%s/b.c", root);
    pf_assert_true(array_contains(&dst, expect));
    pstrarray_free(&dst);

    snprintf(pattern, sizeof(pattern), "%s/*/*.c", root);
    pat = wrap(pattern);
    pf_assert_ok(pstrglob(&dst, &pat, 0, NULL));
    pf_assert(dst.length == 1);
    snprintf(expect, sizeof(expect), "%s/sub/d.c", root);
    pf_assert_true(array_contains(&dst, expect));
    pstrarray_free(&dst);

    snprintf(pattern, sizeof(pattern), "%s/*.nomatch", root);
    pat = wrap(pattern);
    pf_assert(PSTRING_ENOENT == pstrglob(&dst, &pat, 0, NULL));

    pat = wrap(pattern);
    pf_assert_ok(pstrglob(&dst, &pat, PSTRGLOB_NOCHECK, NULL));
    pf_assert(dst.length == 1);
    pf_assert_true(array_contains(&dst, pattern));
    pstrarray_free(&dst);

    remove_tree(root);
    return 0;
}

int test_glob_errors(int seed, int rep) {
    pstrarray_t dst = { 0 };
    pf_assert(PSTRTHROW_EINVAL == pstrglob(NULL, &PSTRWRAP("*"), 0, NULL));
    pf_assert(PSTRTHROW_EINVAL == pstrglob(&dst, NULL, 0, NULL));
    return 0;
}

static int brace_contains(pstrarray_t *arr, const char *needle) {
    for (size_t i = 0; i < arr->length; i++) {
        if (pstrequals(&arr->items[i], needle, 0))
            return 1;
    }
    return 0;
}

int test_brace_basic(int seed, int rep) {
    pstrarray_t out = { 0 };

    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("file.{c,h}"), NULL));
    pf_assert(out.length == 2);
    pf_assert_true(brace_contains(&out, "file.c"));
    pf_assert_true(brace_contains(&out, "file.h"));
    pstrarray_free(&out);

    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("{a,b}{c,d}"), NULL));
    pf_assert(out.length == 4);
    pf_assert_true(brace_contains(&out, "ac"));
    pf_assert_true(brace_contains(&out, "ad"));
    pf_assert_true(brace_contains(&out, "bc"));
    pf_assert_true(brace_contains(&out, "bd"));
    pstrarray_free(&out);

    /* a trailing empty alternative is preserved */
    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("{a,}"), NULL));
    pf_assert(out.length == 2);
    pf_assert_true(brace_contains(&out, "a"));
    pf_assert_true(brace_contains(&out, ""));
    pstrarray_free(&out);

    /* no braces at all: the pattern passes through unchanged */
    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("plain"), NULL));
    pf_assert(out.length == 1);
    pf_assert_true(brace_contains(&out, "plain"));
    pstrarray_free(&out);

    return 0;
}

int test_brace_nested(int seed, int rep) {
    pstrarray_t out = { 0 };

    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("a{b,c{d,e}}f"), NULL));
    pf_assert(out.length == 3);
    pf_assert_true(brace_contains(&out, "abf"));
    pf_assert_true(brace_contains(&out, "acdf"));
    pf_assert_true(brace_contains(&out, "acef"));
    pstrarray_free(&out);

    return 0;
}

int test_brace_no_comma(int seed, int rep) {
    pstrarray_t out = { 0 };

    /* a brace group with no top-level comma is not a valid brace
       expression, so its braces stay literal ... */
    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("{lonely}"), NULL));
    pf_assert(out.length == 1);
    pf_assert_true(brace_contains(&out, "{lonely}"));
    pstrarray_free(&out);

    /* ... but an inner comma-bearing pair inside a comma-less outer
       pair still expands, since the outer '{' just becomes literal
       text and scanning continues right after it (matching the common
       shell behaviour for this exact case) */
    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("{a{b,c}}"), NULL));
    pf_assert(out.length == 2);
    pf_assert_true(brace_contains(&out, "{ab}"));
    pf_assert_true(brace_contains(&out, "{ac}"));
    pstrarray_free(&out);

    return 0;
}

int test_brace_escape(int seed, int rep) {
    pstrarray_t out = { 0 };

    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("a\\{b,c}d"), NULL));
    pf_assert(out.length == 1);
    pf_assert_true(brace_contains(&out, "a{b,c}d"));
    pstrarray_free(&out);

    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("{a\\,b,c}"), NULL));
    pf_assert(out.length == 2);
    pf_assert_true(brace_contains(&out, "a,b"));
    pf_assert_true(brace_contains(&out, "c"));
    pstrarray_free(&out);

    return 0;
}

int test_brace_multiple_groups(int seed, int rep) {
    pstrarray_t out = { 0 };

    /* results accumulate across repeated calls into the same dst,
       same as pstrglob */
    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("{a,b}"), NULL));
    pf_assert_ok(pstrbrace(&out, &PSTRWRAP("{c,d}"), NULL));
    pf_assert(out.length == 4);
    pf_assert_true(brace_contains(&out, "a"));
    pf_assert_true(brace_contains(&out, "b"));
    pf_assert_true(brace_contains(&out, "c"));
    pf_assert_true(brace_contains(&out, "d"));
    pstrarray_free(&out);

    return 0;
}

int test_brace_errors(int seed, int rep) {
    pstrarray_t out = { 0 };
    pf_assert(PSTRTHROW_EINVAL == pstrbrace(NULL, &PSTRWRAP("{a,b}"), NULL));
    pf_assert(PSTRTHROW_EINVAL == pstrbrace(&out, NULL, NULL));
    return 0;
}

const pf_test_t suite_glob[] = {
    { test_glob_match_basic, "/pstring/glob/match_basic" },
    { test_glob_match_brackets, "/pstring/glob/match_brackets" },
    { test_glob_match_period, "/pstring/glob/match_period" },
    { test_glob_match_escape, "/pstring/glob/match_escape" },
    { test_glob_match_errors, "/pstring/glob/match_errors" },
    { test_glob_filesystem, "/pstring/glob/filesystem" },
    { test_glob_errors, "/pstring/glob/errors" },
    { test_brace_basic, "/pstring/glob/brace_basic" },
    { test_brace_nested, "/pstring/glob/brace_nested" },
    { test_brace_no_comma, "/pstring/glob/brace_no_comma" },
    { test_brace_escape, "/pstring/glob/brace_escape" },
    { test_brace_multiple_groups, "/pstring/glob/brace_multiple_groups" },
    { test_brace_errors, "/pstring/glob/brace_errors" },
    { 0 },
};
