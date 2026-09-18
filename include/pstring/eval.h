/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_EVAL_H
#define PSTRING_EVAL_H

#ifndef PSTR_API
    #define PSTR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** ## NAME

    **pstring-eval** - shell-like arithmetic expansion, i.e. what runs
    inside `$((...))` in POSIX shells.

    ## DESCRIPTION

    `pstreval` parses and evaluates a single arithmetic expression using
    64-bit signed integer arithmetic throughout (matching `bash`'s own
    `$((...))`, which has no floating point). Supported grammar, from
    lowest to highest precedence:

    - `,` (comma, evaluates both sides, yields the right)
    - `= += -= *= /= %= &= ^= |= <<= >>=` (assignment, right-associative)
    - `?:` (ternary, right-associative)
    - `||`
    - `&&`
    - `|`
    - `^`
    - `&`
    - `== !=`
    - `< <= > >=`
    - `<< >>`
    - `+ -`
    - `* / %`
    - unary `+ - ! ~`
    - `( expr )`
    - integer literals: decimal, `0x`/`0X` hex, `0` + octal digits
    - identifiers, resolved via the `resolve`/`assign` callbacks

    Division and modulo by zero are reported as `PSTRING_ERANGE` rather
    than invoking undefined behaviour.

    [TOC]

    ## REFERENCE
**/

typedef struct pstring_t pstring_t;

/** Called for every bare identifier the expression references. Return
    `PSTRING_OK` and write the variable's current value to `*out` (`0` if
    unset is a reasonable default, matching shell semantics), or return an
    error to abort evaluation. May be `NULL`, in which case every
    identifier evaluates to `0`. **/
typedef int pstreval_get_fn(long long *out, const pstring_t *name, void *user);

/** Called when the expression assigns to an identifier (`x = 1`, `x += 2`,
    a leading/trailing `++`/`--`, ...). May be `NULL`, in which case
    assignment expressions fail with `PSTRING_EINVAL`. **/
typedef int pstreval_set_fn(const pstring_t *name, long long value, void *user);

/** Evaluates `expr` and writes the result to `*out`. `get`/`set` resolve
    and store variable references; either may be `NULL` per their
    documentation above. `user` is passed through to both unchanged.

    Possible error codes: PSTRING_EINVAL (parse error, or assignment with
    no `set` callback), PSTRING_ERANGE (division or modulo by zero), plus
    whatever `get`/`set` themselves return.
**/
PSTR_API int pstreval(
    long long *out,
    const pstring_t *expr,
    pstreval_get_fn *get,
    pstreval_set_fn *set,
    void *user
);

#ifdef __cplusplus
}
#endif

#endif
