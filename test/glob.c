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

const pf_test_t suite_glob[] = {
    { test_glob_match_basic, "/pstring/glob/match_basic" },
    { test_glob_match_brackets, "/pstring/glob/match_brackets" },
    { test_glob_match_period, "/pstring/glob/match_period" },
    { test_glob_match_escape, "/pstring/glob/match_escape" },
    { test_glob_match_errors, "/pstring/glob/match_errors" },
    { test_glob_filesystem, "/pstring/glob/filesystem" },
    { test_glob_errors, "/pstring/glob/errors" },
    { 0 },
};
