/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pf_assert.h>
#include <pf_test.h>

#include <pstring/eval.h>
#include <pstring/pstring.h>

#define MAX_VARS 8

struct env {
    pstring_t names[MAX_VARS];
    long long values[MAX_VARS];
    int count;
};

static int env_get(long long *out, const pstring_t *name, void *user) {
    struct env *env = user;
    for (int i = 0; i < env->count; i++) {
        if (pstrequal(&env->names[i], name)) {
            *out = env->values[i];
            return PSTRING_OK;
        }
    }
    *out = 0;
    return PSTRING_OK;
}

static int env_set(const pstring_t *name, long long value, void *user) {
    struct env *env = user;
    for (int i = 0; i < env->count; i++) {
        if (pstrequal(&env->names[i], name)) {
            env->values[i] = value;
            return PSTRING_OK;
        }
    }
    if (env->count >= MAX_VARS)
        return PSTRING_ENOMEM;
    env->names[env->count] = *name;
    env->values[env->count] = value;
    env->count++;
    return PSTRING_OK;
}

static long long eval_str(
    const char *expr, pstreval_get_fn *get, pstreval_set_fn *set, void *user
) {
    long long out = 0xdeadbeef;
    pstring_t str;
    pstrwrap(&str, (char *)expr, 0, 0);
    int rc = pstreval(&out, &str, get, set, user);
    pf_assert_ok(rc);
    return out;
}

int test_eval_literals(int seed, int rep) {
    pf_assert(0 == eval_str("0", NULL, NULL, NULL));
    pf_assert(42 == eval_str("42", NULL, NULL, NULL));
    pf_assert(255 == eval_str("0xff", NULL, NULL, NULL));
    pf_assert(255 == eval_str("0XFF", NULL, NULL, NULL));
    pf_assert(8 == eval_str("010", NULL, NULL, NULL));
    pf_assert(0 == eval_str("  0  ", NULL, NULL, NULL));
    return 0;
}

int test_eval_arithmetic(int seed, int rep) {
    pf_assert(7 == eval_str("3 + 4", NULL, NULL, NULL));
    pf_assert(-1 == eval_str("3 - 4", NULL, NULL, NULL));
    pf_assert(12 == eval_str("3 * 4", NULL, NULL, NULL));
    pf_assert(3 == eval_str("10 / 3", NULL, NULL, NULL));
    pf_assert(1 == eval_str("10 % 3", NULL, NULL, NULL));
    pf_assert(14 == eval_str("2 + 3 * 4", NULL, NULL, NULL));
    pf_assert(20 == eval_str("(2 + 3) * 4", NULL, NULL, NULL));
    pf_assert(-5 == eval_str("-5", NULL, NULL, NULL));
    pf_assert(5 == eval_str("- -5", NULL, NULL, NULL));
    pf_assert(5 == eval_str("+5", NULL, NULL, NULL));
    return 0;
}

int test_eval_division_errors(int seed, int rep) {
    long long out;
    pf_assert(
        PSTRING_ERANGE == pstreval(&out, &PSTRWRAP("1 / 0"), NULL, NULL, NULL)
    );
    pf_assert(
        PSTRING_ERANGE == pstreval(&out, &PSTRWRAP("1 % 0"), NULL, NULL, NULL)
    );
    return 0;
}

int test_eval_bitwise(int seed, int rep) {
    pf_assert(6 == eval_str("2 | 4", NULL, NULL, NULL));
    pf_assert(0 == eval_str("2 & 4", NULL, NULL, NULL));
    pf_assert(6 == eval_str("2 ^ 4", NULL, NULL, NULL));
    pf_assert(-1 == eval_str("~0", NULL, NULL, NULL));
    pf_assert(8 == eval_str("1 << 3", NULL, NULL, NULL));
    pf_assert(1 == eval_str("8 >> 3", NULL, NULL, NULL));
    return 0;
}

int test_eval_comparisons(int seed, int rep) {
    pf_assert(1 == eval_str("3 < 4", NULL, NULL, NULL));
    pf_assert(0 == eval_str("4 < 3", NULL, NULL, NULL));
    pf_assert(1 == eval_str("4 <= 4", NULL, NULL, NULL));
    pf_assert(1 == eval_str("4 >= 4", NULL, NULL, NULL));
    pf_assert(1 == eval_str("3 == 3", NULL, NULL, NULL));
    pf_assert(1 == eval_str("3 != 4", NULL, NULL, NULL));
    pf_assert(0 == eval_str("!3", NULL, NULL, NULL));
    pf_assert(1 == eval_str("!0", NULL, NULL, NULL));
    return 0;
}

int test_eval_logical(int seed, int rep) {
    pf_assert(1 == eval_str("1 && 1", NULL, NULL, NULL));
    pf_assert(0 == eval_str("1 && 0", NULL, NULL, NULL));
    pf_assert(1 == eval_str("0 || 1", NULL, NULL, NULL));
    pf_assert(0 == eval_str("0 || 0", NULL, NULL, NULL));
    /* short-circuit still runs the RHS in this implementation, but the
       overall value must still reflect the shell truth table */
    pf_assert(1 == eval_str("1 ? 1 : 0", NULL, NULL, NULL));
    pf_assert(0 == eval_str("0 ? 1 : 0", NULL, NULL, NULL));
    /* both branches are always parsed and evaluated (no short-circuit),
       matching this implementation's straightforward recursive design */
    pf_assert(2 == eval_str("1 ? 2 : 3", NULL, NULL, NULL));
    return 0;
}

int test_eval_comma(int seed, int rep) {
    pf_assert(4 == eval_str("1, 2, 3, 4", NULL, NULL, NULL));
    return 0;
}

int test_eval_variables(int seed, int rep) {
    struct env env = { .count = 0 };
    pf_assert(0 == eval_str("undefined_var", env_get, env_set, &env));

    eval_str("x = 5", env_get, env_set, &env);
    pf_assert(5 == eval_str("x", env_get, env_set, &env));

    pf_assert(8 == eval_str("x + 3", env_get, env_set, &env));

    eval_str("x += 10", env_get, env_set, &env);
    pf_assert(15 == eval_str("x", env_get, env_set, &env));

    eval_str("x -= 5", env_get, env_set, &env);
    pf_assert(10 == eval_str("x", env_get, env_set, &env));

    eval_str("x *= 3", env_get, env_set, &env);
    pf_assert(30 == eval_str("x", env_get, env_set, &env));

    eval_str("x /= 4", env_get, env_set, &env);
    pf_assert(7 == eval_str("x", env_get, env_set, &env));

    eval_str("x %= 4", env_get, env_set, &env);
    pf_assert(3 == eval_str("x", env_get, env_set, &env));

    eval_str("y = x = 9", env_get, env_set, &env);
    pf_assert(9 == eval_str("x", env_get, env_set, &env));
    pf_assert(9 == eval_str("y", env_get, env_set, &env));

    return 0;
}

int test_eval_incdec(int seed, int rep) {
    struct env env = { .count = 0 };
    eval_str("x = 5", env_get, env_set, &env);

    pf_assert(5 == eval_str("x++", env_get, env_set, &env));
    pf_assert(6 == eval_str("x", env_get, env_set, &env));

    pf_assert(7 == eval_str("++x", env_get, env_set, &env));
    pf_assert(7 == eval_str("x", env_get, env_set, &env));

    pf_assert(7 == eval_str("x--", env_get, env_set, &env));
    pf_assert(6 == eval_str("x", env_get, env_set, &env));

    pf_assert(5 == eval_str("--x", env_get, env_set, &env));
    pf_assert(5 == eval_str("x", env_get, env_set, &env));

    return 0;
}

int test_eval_no_callbacks(int seed, int rep) {
    long long out;
    pf_assert(0 == eval_str("unset_variable", NULL, NULL, NULL));
    pf_assert(
        PSTRING_EINVAL == pstreval(&out, &PSTRWRAP("x = 5"), NULL, NULL, NULL)
    );
    return 0;
}

int test_eval_errors(int seed, int rep) {
    long long out;
    pf_assert(
        PSTRING_EINVAL == pstreval(NULL, &PSTRWRAP("1"), NULL, NULL, NULL)
    );
    pf_assert(PSTRING_EINVAL == pstreval(&out, NULL, NULL, NULL, NULL));
    pf_assert(
        PSTRING_EINVAL == pstreval(&out, &PSTRWRAP(""), NULL, NULL, NULL)
    );
    pf_assert(
        PSTRING_EINVAL == pstreval(&out, &PSTRWRAP("1 +"), NULL, NULL, NULL)
    );
    pf_assert(
        PSTRING_EINVAL == pstreval(&out, &PSTRWRAP("(1"), NULL, NULL, NULL)
    );
    pf_assert(
        PSTRING_EINVAL == pstreval(&out, &PSTRWRAP("1 2"), NULL, NULL, NULL)
    );
    pf_assert(
        PSTRING_EINVAL == pstreval(&out, &PSTRWRAP("1 ? 2"), NULL, NULL, NULL)
    );
    pf_assert(
        PSTRING_EINVAL == pstreval(&out, &PSTRWRAP("1 = 2"), NULL, NULL, NULL)
    );
    return 0;
}

const pf_test_t suite_eval[] = {
    { test_eval_literals, "/pstring/eval/literals" },
    { test_eval_arithmetic, "/pstring/eval/arithmetic" },
    { test_eval_division_errors, "/pstring/eval/division_errors" },
    { test_eval_bitwise, "/pstring/eval/bitwise" },
    { test_eval_comparisons, "/pstring/eval/comparisons" },
    { test_eval_logical, "/pstring/eval/logical" },
    { test_eval_comma, "/pstring/eval/comma" },
    { test_eval_variables, "/pstring/eval/variables" },
    { test_eval_incdec, "/pstring/eval/incdec" },
    { test_eval_no_callbacks, "/pstring/eval/no_callbacks" },
    { test_eval_errors, "/pstring/eval/errors" },
    { 0 },
};
