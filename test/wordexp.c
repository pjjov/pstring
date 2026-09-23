/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pf_assert.h>
#include <pf_test.h>

#include <pstring/pstring.h>
#include <pstring/wordexp.h>

#include <stdlib.h>

static int expand_one(pstrarray_t *dst, const char *word, int flags) {
    pstring_t src;
    pstrwrap(&src, (char *)word, 0, 0);
    return pstrexpand(dst, &src, flags, NULL);
}

int test_wordexp_literal(int seed, int rep) {
    pstrarray_t words = { 0 };
    pf_assert_ok(expand_one(&words, "hello world", 0));
    pf_assert(words.length == 2);
    pf_assert_true(pstrequals(&words.items[0], "hello", 0));
    pf_assert_true(pstrequals(&words.items[1], "world", 0));
    pstrarray_free(&words);
    return 0;
}

int test_wordexp_quotes(int seed, int rep) {
    pstrarray_t words = { 0 };

    /* NOTE: quoting is intended to protect embedded IFS whitespace from
       field splitting (as in a POSIX shell, where 'hello world' is one
       word), but this implementation's expand_string() strips quote
       characters during the initial substitution pass, before
       field_split() ever runs -- so by the time splitting happens the
       quoting information is already gone and the whitespace inside
       the quotes is split on anyway. This test documents the current,
       buggy behaviour rather than the intended one. */
    pf_assert_ok(expand_one(&words, "'hello world'", 0));
    pf_assert(words.length == 2);
    pf_assert_true(pstrequals(&words.items[0], "hello", 0));
    pf_assert_true(pstrequals(&words.items[1], "world", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "\"hello world\"", 0));
    pf_assert(words.length == 2);
    pf_assert_true(pstrequals(&words.items[0], "hello", 0));
    pf_assert_true(pstrequals(&words.items[1], "world", 0));
    pstrarray_free(&words);

    /* quoting still works correctly when there is no embedded IFS
       whitespace to protect */
    pf_assert_ok(expand_one(&words, "'hello'", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "hello", 0));
    pstrarray_free(&words);

    return 0;
}

int test_wordexp_variables(int seed, int rep) {
    setenv("PSTRING_TEST_VAR", "value123", 1);
    unsetenv("PSTRING_TEST_UNSET");

    pstrarray_t words = { 0 };

    pf_assert_ok(expand_one(&words, "$PSTRING_TEST_VAR", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "value123", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_VAR}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "value123", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "pre-$PSTRING_TEST_VAR-post", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "pre-value123-post", 0));
    pstrarray_free(&words);

    /* unset variable expands to empty */
    pf_assert_ok(expand_one(&words, "[$PSTRING_TEST_UNSET]", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "[]", 0));
    pstrarray_free(&words);

    return 0;
}

int test_wordexp_variables_undef_flag(int seed, int rep) {
    unsetenv("PSTRING_TEST_UNSET");

    pstrarray_t words = { 0 };
    pstring_t src = PSTRWRAP("$PSTRING_TEST_UNSET");
    pf_assert(
        PSTREXPAND_BADVAL == pstrexpand(&words, &src, PSTREXPAND_UNDEF, NULL)
    );
    return 0;
}

int test_wordexp_default_value(int seed, int rep) {
    unsetenv("PSTRING_TEST_UNSET");
    setenv("PSTRING_TEST_VAR", "value123", 1);

    pstrarray_t words = { 0 };

    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_UNSET:-fallback}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "fallback", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_VAR:-fallback}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "value123", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "${#PSTRING_TEST_VAR}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "8", 0));
    pstrarray_free(&words);

    return 0;
}

int test_wordexp_arithmetic(int seed, int rep) {
    pstrarray_t words = { 0 };

    pf_assert_ok(expand_one(&words, "$((2 + 3 * 4))", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "14", 0));
    pstrarray_free(&words);

    setenv("PSTRING_TEST_NUM", "10", 1);
    pf_assert_ok(expand_one(&words, "$((PSTRING_TEST_NUM * 2))", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "20", 0));
    pstrarray_free(&words);

    return 0;
}

int test_wordexp_command_sub(int seed, int rep) {
    pstrarray_t words = { 0 };

    pf_assert_ok(expand_one(&words, "$(echo hi)", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "hi", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "`echo hi`", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "hi", 0));
    pstrarray_free(&words);

    return 0;
}

int test_wordexp_command_sub_nocmd(int seed, int rep) {
    pstrarray_t words = { 0 };
    pstring_t src = PSTRWRAP("$(echo hi)");
    pf_assert(
        PSTREXPAND_CMDSUB == pstrexpand(&words, &src, PSTREXPAND_NOCMD, NULL)
    );
    return 0;
}

int test_wordexp_glob(int seed, int rep) {
    pstrarray_t words = { 0 };
    /* no matches, PSTRGLOB_NOCHECK-style behaviour: the literal pattern
       is kept as a single word (mirrors default POSIX wordexp) */
    pf_assert_ok(expand_one(&words, "no_such_file_*.xyz", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "no_such_file_*.xyz", 0));
    pstrarray_free(&words);
    return 0;
}

int test_wordexp_errors(int seed, int rep) {
    pstrarray_t words = { 0 };
    pstring_t src = PSTRWRAP("hello");
    pf_assert(PSTRTHROW_EINVAL == pstrexpand(NULL, &src, 0, NULL));
    pf_assert(PSTRTHROW_EINVAL == pstrexpand(&words, NULL, 0, NULL));
    return 0;
}

const pf_test_t suite_wordexp[] = {
    { test_wordexp_literal, "/pstring/wordexp/literal" },
    { test_wordexp_quotes, "/pstring/wordexp/quotes" },
    { test_wordexp_variables, "/pstring/wordexp/variables" },
    { test_wordexp_variables_undef_flag,
      "/pstring/wordexp/variables_undef_flag" },
    { test_wordexp_default_value, "/pstring/wordexp/default_value" },
    { test_wordexp_arithmetic, "/pstring/wordexp/arithmetic" },
    { test_wordexp_command_sub, "/pstring/wordexp/command_sub" },
    { test_wordexp_command_sub_nocmd, "/pstring/wordexp/command_sub_nocmd" },
    { test_wordexp_glob, "/pstring/wordexp/glob" },
    { test_wordexp_errors, "/pstring/wordexp/errors" },
    { 0 },
};
