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

    pf_assert_ok(expand_one(&words, "'hello world'", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "hello world", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "\"hello world\"", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "hello world", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "'hello'", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "hello", 0));
    pstrarray_free(&words);

    /* quoting still allows partial-word concatenation: unquoted and
       quoted text glued together with no space between stays one
       word, and the quoted part's internal whitespace is preserved */
    pf_assert_ok(expand_one(&words, "a'b c'd", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "ab cd", 0));
    pstrarray_free(&words);

    /* two separate quoted words, actually separated by unquoted IFS
       whitespace, still split into two fields */
    pf_assert_ok(expand_one(&words, "'a' 'b'", 0));
    pf_assert(words.length == 2);
    pf_assert_true(pstrequals(&words.items[0], "a", 0));
    pf_assert_true(pstrequals(&words.items[1], "b", 0));
    pstrarray_free(&words);

    /* an empty quoted word still produces a (single, empty) field */
    pf_assert_ok(expand_one(&words, "''", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "", 0));
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

int test_wordexp_trim(int seed, int rep) {
    setenv("PSTRING_TEST_PATH", "/path/to/file.tar.gz", 1);
    setenv("PSTRING_TEST_UNSET_TRIM", "", 1);
    unsetenv("PSTRING_TEST_UNSET_TRIM");

    pstrarray_t words = { 0 };

    /* '#' / '##': shortest / longest matching prefix removed */
    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_PATH#*/}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "path/to/file.tar.gz", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_PATH##*/}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "file.tar.gz", 0));
    pstrarray_free(&words);

    /* '%' / '%%': shortest / longest matching suffix removed */
    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_PATH%.*}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "/path/to/file.tar", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_PATH%%.*}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "/path/to/file", 0));
    pstrarray_free(&words);

    /* a pattern that doesn't match anything leaves the value alone */
    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_PATH#nomatch}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "/path/to/file.tar.gz", 0));
    pstrarray_free(&words);

    /* '*' can match zero characters, so the shortest-match '#'/'%'
       forms with a bare '*' pattern are a no-op */
    setenv("PSTRING_TEST_WORD", "hello", 1);
    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_WORD#*}", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "hello", 0));
    pstrarray_free(&words);

    /* an unset variable trims to an empty value, not an error */
    pf_assert_ok(expand_one(&words, "[${PSTRING_TEST_UNSET_TRIM#*}]", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "[]", 0));
    pstrarray_free(&words);

    return 0;
}

int test_wordexp_trim_undef_flag(int seed, int rep) {
    unsetenv("PSTRING_TEST_UNSET_TRIM");

    pstrarray_t words = { 0 };
    pstring_t src = PSTRWRAP("${PSTRING_TEST_UNSET_TRIM#*}");
    pf_assert(
        PSTREXPAND_BADVAL == pstrexpand(&words, &src, PSTREXPAND_UNDEF, NULL)
    );
    return 0;
}

int test_wordexp_brace(int seed, int rep) {
    pstrarray_t words = { 0 };

    pf_assert_ok(expand_one(&words, "file.{c,h}", 0));
    pf_assert(words.length == 2);
    pf_assert_true(pstrequals(&words.items[0], "file.c", 0));
    pf_assert_true(pstrequals(&words.items[1], "file.h", 0));
    pstrarray_free(&words);

    pf_assert_ok(expand_one(&words, "{a,b}{c,d}", 0));
    pf_assert(words.length == 4);
    pf_assert_true(pstrequals(&words.items[0], "ac", 0));
    pf_assert_true(pstrequals(&words.items[1], "ad", 0));
    pf_assert_true(pstrequals(&words.items[2], "bc", 0));
    pf_assert_true(pstrequals(&words.items[3], "bd", 0));
    pstrarray_free(&words);

    /* a quoted brace group is not expanded */
    pf_assert_ok(expand_one(&words, "'{a,b}'", 0));
    pf_assert(words.length == 1);
    pf_assert_true(pstrequals(&words.items[0], "{a,b}", 0));
    pstrarray_free(&words);

    /* each brace alternative still goes through normal field-splitting,
       so an embedded unquoted space in one alternative still splits */
    pf_assert_ok(expand_one(&words, "pre{a,b c}post", 0));
    pf_assert(words.length == 3);
    pf_assert_true(pstrequals(&words.items[0], "preapost", 0));
    pf_assert_true(pstrequals(&words.items[1], "preb", 0));
    pf_assert_true(pstrequals(&words.items[2], "cpost", 0));
    pstrarray_free(&words);

    /* variable expansion still runs on each brace alternative */
    setenv("PSTRING_TEST_BRACE_VAR", "X", 1);
    pf_assert_ok(expand_one(&words, "${PSTRING_TEST_BRACE_VAR}{1,2}", 0));
    pf_assert(words.length == 2);
    pf_assert_true(pstrequals(&words.items[0], "X1", 0));
    pf_assert_true(pstrequals(&words.items[1], "X2", 0));
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
    { test_wordexp_trim, "/pstring/wordexp/trim" },
    { test_wordexp_trim_undef_flag, "/pstring/wordexp/trim_undef_flag" },
    { test_wordexp_brace, "/pstring/wordexp/brace" },
    { test_wordexp_errors, "/pstring/wordexp/errors" },
    { 0 },
};
