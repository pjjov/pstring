/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/eval.h>
#include <pstring/pstring.h>

#include <pf_ctype.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/** A straightforward recursive-descent parser over the token stream,
    evaluating as it goes rather than building an AST -- arithmetic
    expansions are short-lived and small, so there is no reuse to justify
    the extra allocation a separate parse tree would need. **/
struct eval_state {
    const char *p;
    const char *end;
    pstreval_get_fn *get;
    pstreval_set_fn *set;
    void *user;
    int error;
};

static void skip_ws(struct eval_state *e) {
    while (e->p < e->end && pf_isspace((unsigned char)*e->p))
        e->p++;
}

static int peek(struct eval_state *e, const char *tok) {
    skip_ws(e);
    size_t len = strlen(tok);
    if ((size_t)(e->end - e->p) < len)
        return 0;
    if (memcmp(e->p, tok, len) != 0)
        return 0;

    /* Don't let e.g. "<" match the start of "<<", or "+" the start of
       "++"/"+=". This must NOT fire at end-of-input: `strchr(s, '\0')`
       always succeeds (it finds `s`'s own terminator), so checking
       `next` without first confirming one actually exists made every
       operator that can end an expression -- "a++" with nothing after
       it, "a - -1" at the very end of the string, etc. -- unmatchable. */
    if (e->end - e->p == (ptrdiff_t)len)
        return 1;

    char next = e->p[len];
    if (strchr("+-*/%&|^<>=!", tok[len - 1]) && strchr("+-*/%&|^<>=", next))
        return 0;
    return 1;
}

static int accept(struct eval_state *e, const char *tok) {
    if (!peek(e, tok))
        return 0;
    e->p += strlen(tok);
    return 1;
}

static int is_ident_start(char c) {
    return pf_isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c) {
    return pf_isalnum((unsigned char)c) || c == '_';
}

static long long eval_ternary(struct eval_state *e);
static long long eval_assign(struct eval_state *e);

/** Reads a bare identifier (variable name) starting at the current
    position, returning its extent. Caller has already confirmed
    `is_ident_start` at `e->p`. **/
static void read_ident(
    struct eval_state *e, const char **start, const char **stop
) {
    *start = e->p;
    while (e->p < e->end && is_ident_char(*e->p))
        e->p++;
    *stop = e->p;
}

static long long get_var(
    struct eval_state *e, const char *start, const char *stop
) {
    pstring_t name;
    pstrrange(&name, NULL, start, stop);

    if (!e->get)
        return 0;

    long long value = 0;
    int rc = e->get(&value, &name, e->user);
    if (rc && !e->error)
        e->error = rc;
    return value;
}

static void set_var(
    struct eval_state *e, const char *start, const char *stop, long long value
) {
    pstring_t name;
    pstrrange(&name, NULL, start, stop);

    if (!e->set) {
        if (!e->error)
            e->error = PSTRING_EINVAL;
        return;
    }

    int rc = e->set(&name, value, e->user);
    if (rc && !e->error)
        e->error = rc;
}

/** Parses a primary expression: literal, parenthesised sub-expression,
    or identifier (with an optional pre/post `++`/`--`, which shells
    support inside arithmetic contexts). **/
static long long eval_primary(struct eval_state *e) {
    skip_ws(e);

    if (accept(e, "(")) {
        long long v = eval_assign(e);
        skip_ws(e);
        if (!accept(e, ")") && !e->error)
            e->error = PSTRING_EINVAL;
        return v;
    }

    if (accept(e, "++") || accept(e, "--")) {
        int inc = e->p[-1] == '+' ? 1 : -1;
        skip_ws(e);
        if (!is_ident_start(*e->p)) {
            if (!e->error)
                e->error = PSTRING_EINVAL;
            return 0;
        }
        const char *start, *stop;
        read_ident(e, &start, &stop);
        long long v = get_var(e, start, stop) + inc;
        set_var(e, start, stop, v);
        return v;
    }

    if (e->p < e->end && (pf_isdigit((unsigned char)*e->p))) {
        char *stop;
        long long v = strtoll(e->p, &stop, 0);
        e->p = stop;
        return v;
    }

    if (e->p < e->end && is_ident_start(*e->p)) {
        const char *start, *stop;
        read_ident(e, &start, &stop);

        skip_ws(e);
        if (accept(e, "++") || accept(e, "--")) {
            int inc = e->p[-1] == '+' ? 1 : -1;
            long long v = get_var(e, start, stop);
            set_var(e, start, stop, v + inc);
            return v; /* post-increment yields the OLD value */
        }

        return get_var(e, start, stop);
    }

    if (!e->error)
        e->error = PSTRING_EINVAL;
    return 0;
}

static long long eval_unary(struct eval_state *e) {
    skip_ws(e);
    if (accept(e, "-"))
        return -eval_unary(e);
    if (accept(e, "+"))
        return eval_unary(e);
    if (accept(e, "!"))
        return !eval_unary(e);
    if (accept(e, "~"))
        return ~eval_unary(e);
    return eval_primary(e);
}

static long long checked_div(struct eval_state *e, long long a, long long b) {
    if (b == 0) {
        if (!e->error)
            e->error = PSTRING_ERANGE;
        return 0;
    }
    /* INT64_MIN / -1 overflows a two's-complement signed division */
    if (a == LLONG_MIN && b == -1)
        return LLONG_MIN;
    return a / b;
}

static long long checked_mod(struct eval_state *e, long long a, long long b) {
    if (b == 0) {
        if (!e->error)
            e->error = PSTRING_ERANGE;
        return 0;
    }
    if (a == LLONG_MIN && b == -1)
        return 0;
    return a % b;
}

static long long eval_mul(struct eval_state *e) {
    long long v = eval_unary(e);
    for (;;) {
        skip_ws(e);
        if (accept(e, "*"))
            v = v * eval_unary(e);
        else if (accept(e, "/"))
            v = checked_div(e, v, eval_unary(e));
        else if (accept(e, "%"))
            v = checked_mod(e, v, eval_unary(e));
        else
            return v;
    }
}

static long long eval_add(struct eval_state *e) {
    long long v = eval_mul(e);
    for (;;) {
        skip_ws(e);
        if (accept(e, "+"))
            v = v + eval_mul(e);
        else if (accept(e, "-"))
            v = v - eval_mul(e);
        else
            return v;
    }
}

static long long eval_shift(struct eval_state *e) {
    long long v = eval_add(e);
    for (;;) {
        skip_ws(e);
        if (accept(e, "<<"))
            v = (long long)((unsigned long long)v << (eval_add(e) & 63));
        else if (accept(e, ">>"))
            v = v >> (eval_add(e) & 63);
        else
            return v;
    }
}

static long long eval_relational(struct eval_state *e) {
    long long v = eval_shift(e);
    for (;;) {
        skip_ws(e);
        if (accept(e, "<="))
            v = v <= eval_shift(e);
        else if (accept(e, ">="))
            v = v >= eval_shift(e);
        else if (accept(e, "<"))
            v = v < eval_shift(e);
        else if (accept(e, ">"))
            v = v > eval_shift(e);
        else
            return v;
    }
}

static long long eval_equality(struct eval_state *e) {
    long long v = eval_relational(e);
    for (;;) {
        skip_ws(e);
        if (accept(e, "=="))
            v = v == eval_relational(e);
        else if (accept(e, "!="))
            v = v != eval_relational(e);
        else
            return v;
    }
}

static long long eval_bitand(struct eval_state *e) {
    long long v = eval_equality(e);
    while (accept(e, "&"))
        v = v & eval_equality(e);
    return v;
}

static long long eval_bitxor(struct eval_state *e) {
    long long v = eval_bitand(e);
    while (accept(e, "^"))
        v = v ^ eval_bitand(e);
    return v;
}

static long long eval_bitor(struct eval_state *e) {
    long long v = eval_bitxor(e);
    while (accept(e, "|"))
        v = v | eval_bitxor(e);
    return v;
}

static long long eval_and(struct eval_state *e) {
    long long v = eval_bitor(e);
    while (accept(e, "&&")) {
        long long rhs = eval_bitor(e); /* always parsed: side effects */
        v = v && rhs;
    }
    return v;
}

static long long eval_or(struct eval_state *e) {
    long long v = eval_and(e);
    while (accept(e, "||")) {
        long long rhs = eval_and(e);
        v = v || rhs;
    }
    return v;
}

static long long eval_ternary(struct eval_state *e) {
    long long cond = eval_or(e);

    if (accept(e, "?")) {
        long long a = eval_assign(e);
        skip_ws(e);
        if (!accept(e, ":")) {
            if (!e->error)
                e->error = PSTRING_EINVAL;
            return 0;
        }
        long long b = eval_ternary(e);
        return cond ? a : b;
    }

    return cond;
}

/** Assignment is right-associative and binds an identifier that must
    appear literally to the left of the operator -- `a = b = 3` and
    `a += 2` are valid, `1 = 2` is not. Because the grammar is otherwise
    only concerned with values, this peeks ahead for `ident <ws>* op=`
    before committing to the assignment interpretation. **/
static long long eval_assign(struct eval_state *e) {
    const char *save = e->p;

    skip_ws(e);
    if (is_ident_start(*e->p)) {
        const char *start, *stop;
        read_ident(e, &start, &stop);
        skip_ws(e);

        static const char *const compound[] = {
            "+=", "-=", "*=", "/=", "%=", "&=", "^=", "|=", "<<=", ">>=", NULL
        };

        for (int i = 0; compound[i]; i++) {
            if (accept(e, compound[i])) {
                long long rhs = eval_assign(e);
                long long cur = get_var(e, start, stop);
                long long v;
                switch (compound[i][0]) {
                    /* clang-format off */
                case '+': v = cur + rhs; break;
                case '-': v = cur - rhs; break;
                case '*': v = cur * rhs; break;
                case '/': v = checked_div(e, cur, rhs); break;
                case '%': v = checked_mod(e, cur, rhs); break;
                case '&': v = cur & rhs; break;
                case '^': v = cur ^ rhs; break;
                case '|': v = cur | rhs; break;
                    /* clang-format on */
                default:
                    v = compound[i][1] == '<'
                        ? (long long)((unsigned long long)cur << (rhs & 63))
                        : cur >> (rhs & 63);
                    break;
                }
                set_var(e, start, stop, v);
                return v;
            }
        }

        if (accept(e, "=") && !peek(e, "=")) {
            long long v = eval_assign(e);
            set_var(e, start, stop, v);
            return v;
        }
    }

    /* not an assignment after all -- rewind and parse normally */
    e->p = save;
    return eval_ternary(e);
}

static long long eval_comma(struct eval_state *e) {
    long long v = eval_assign(e);
    while (accept(e, ","))
        v = eval_assign(e);
    return v;
}

int pstreval(
    long long *out,
    const pstring_t *expr,
    pstreval_get_fn *get,
    pstreval_set_fn *set,
    void *user
) {
    if (!out || !expr)
        return PSTRTHROW_EINVAL;

    struct eval_state e = {
        .p = pstrbuf(expr),
        .end = pstrend(expr),
        .get = get,
        .set = set,
        .user = user,
        .error = 0,
    };

    long long result = eval_comma(&e);

    skip_ws(&e);
    if (!e.error && e.p != e.end)
        e.error = PSTRING_EINVAL; /* trailing garbage */

    if (e.error)
        return PSTRTHROW(e.error, NULL);

    *out = result;
    return PSTRING_OK;
}
