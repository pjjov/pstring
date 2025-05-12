/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides assert and static_assert macros from <assert.h>
    and utility assertions for better debugging and specific use cases.
    All assertions provided will print the message without expanding the
    macros, making it especially useful for testing macro-heavy code.

    The 'check' variants turn into a boolean condition when NDEBUG is
    defined, which is useful for parameter checking during development.

    If native assert was found PF_HAS_ASSERT_H will be defined.

    You can define following configuration macros:
    - NDEBUG
    - PF_FUNC_NAME
    - PF_BOOL
    - PF_ASSERT_FAIL
    - PF_ASSERT_ABORT
    - PF_ASSERT_FPRINTF
    - PF_ASSERT_STDERR
    - PF_ASSERT_NO_STRING

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
**/

#ifndef POLYFILL_ASSERT
#define POLYFILL_ASSERT

#ifdef __STDC__
    #define PF_HAS_ASSERT_H
    #include <assert.h>
#elif defined(__has_include)
    #if __has_include(<assert.h>)
        #define PF_HAS_ASSERT_H
        #include <assert.h>
    #endif
#endif

#ifndef PF_FUNC_NAME
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
        #define PF_FUNC_NAME __func__
    #else
        #define PF_FUNC_NAME ((const char *) 0)
    #endif
#endif

#ifndef PF_BOOL
    #define PF_BOOL
    #if defined(bool) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef bool pf_bool;
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
typedef _Bool pf_bool;
    #else
typedef int pf_bool;
    #endif
#endif

#ifndef PF_ASSERT_FAIL

    #ifndef PF_ASSERT_ABORT
        #include <stdlib.h>
        #define PF_ASSERT_ABORT abort
    #endif

    #ifndef PF_ASSERT_FPRINTF
        #include <stdio.h>
        #define PF_ASSERT_FPRINTF fprintf
    #endif

    #ifndef PF_ASSERT_STDERR
        #define PF_ASSERT_STDERR stderr
    #endif

    static void pf_assert_fail(
        const char *msg,
        const char *file,
        int line,
        const char *fn
    ) {
        if (fn) {
            PF_ASSERT_FPRINTF(
                PF_ASSERT_STDERR,
                "%s:%d: %s: %s\n",
                file, line, fn, msg
            );
        } else {
            PF_ASSERT_FPRINTF(
                PF_ASSERT_STDERR,
                "%s:%d: %s\n",
                file, line, msg
            );
        }
        PF_ASSERT_ABORT();
    }

    #define PF_ASSERT_FAIL pf_assert_fail

#endif

static inline pf_bool pf_check_fail(
    const char *msg,
    const char *file,
    int line,
    const char *fn
) {
    PF_ASSERT_FAIL(msg, file, line, fn);
    return 0;
}

/** `pf_assert_always` asserts even if NDEBUG is defined. */
#define pf_assert_always(msg, ...) ((__VA_ARGS__) ? (void)0 \
    : PF_ASSERT_FAIL((msg), __FILE__, __LINE__, PF_FUNC_NAME))

#define pf_check_always(msg, ...)  ((__VA_ARGS__) ? (pf_bool)1 \
    : pf_check_fail((msg), __FILE__, __LINE__, PF_FUNC_NAME))

/*
    When NDEBUG is defined, `pf_assert_or_check` turns into a condition,
    returning 0 or 1. You can use them inside if statements for error
    checking at release and asserting at development.
*/
#ifndef NDEBUG
    #define pf_assert_print pf_assert_always
    #define pf_check_print pf_check_always
#else
    #define pf_assert_print(...) ((void)0)
    #define pf_check(...) ((__VA_ARGS__) ? 1 : 0)
#endif

#define pf_assert(...)          pf_assert_print("Assertion `"  #__VA_ARGS__  "` failed!", __VA_ARGS__)
#define pf_assert_false(...)    pf_assert_print("`"  #__VA_ARGS__  "` is not false!", !(__VA_ARGS__))
#define pf_assert_true(...)     pf_assert_print("`"  #__VA_ARGS__  "` is not true!", __VA_ARGS__)
#define pf_assert_ok(...)       pf_assert_print("`"  #__VA_ARGS__  "` failed!", !(__VA_ARGS__))
#define pf_assert_error(...)    pf_assert_print("`"  #__VA_ARGS__  "` should've failed!", __VA_ARGS__)
#define pf_assert_null(...)     pf_assert_print("`" #__VA_ARGS__ "` is not null!", (__VA_ARGS__) == NULL)
#define pf_assert_not_null(...) pf_assert_print("`" #__VA_ARGS__ "` is null!", (__VA_ARGS__) != NULL)
#define pf_check(...)           pf_check_print("Check `"  #__VA_ARGS__  "` failed!", __VA_ARGS__)
#define pf_check_false(...)     pf_check_print("`"  #__VA_ARGS__  "` is not false!", !(__VA_ARGS__))
#define pf_check_true(...)      pf_check_print("`"  #__VA_ARGS__  "` is not true!", __VA_ARGS__)
#define pf_check_ok(...)        pf_check_print("`"  #__VA_ARGS__  "` failed!", !(__VA_ARGS__))
#define pf_check_error(...)     pf_check_print("`"  #__VA_ARGS__  "` should've failed!", __VA_ARGS__)
#define pf_check_null(...)      pf_check_print("`" #__VA_ARGS__ "` is not null!", (__VA_ARGS__) == NULL)
#define pf_check_not_null(...)  pf_check_print("`" #__VA_ARGS__ "` is null!", (__VA_ARGS__) != NULL)

#ifndef PF_ASSERT_NO_STRING
#include <string.h>
#define pf_assert_memcmp(x, y, size) \
    pf_assert_print("`" #x "` is not equal to `" #y "`!", 0 == memcmp((x), (y), (size)));
#define pf_assert_not_memcmp(x, y, size) \
    pf_assert_print("`" #x "` is equal to `" #y "`!", 0 != memcmp((x), (y), (size)));
#define pf_assert_strcmp(x, y) \
    pf_assert_print("`" #x "` is not equal to `" #y "`!", 0 == strcmp((x), (y)));
#define pf_assert_not_strcmp(x, y) \
    pf_assert_print("`" #x "` is equal to `" #y "`!", 0 != strcmp((x), (y)));
#define pf_check_memcmp(x, y, size) \
    pf_check_print("`" #x "` is not equal to `" #y "`!", 0 == memcmp((x), (y), (size)));
#define pf_check_not_memcmp(x, y, size) \
    pf_check_print("`" #x "` is equal to `" #y "`!", 0 != memcmp((x), (y), (size)));
#define pf_check_strcmp(x, y) \
    pf_check_print("`" #x "` is not equal to `" #y "`!", 0 == strcmp((x), (y)));
#define pf_check_not_strcmp(x, y) \
    pf_check_print("`" #x "` is equal to `" #y "`!", 0 != strcmp((x), (y)));
#endif

#ifndef assert
    #define assert pf_assert;
#endif

#if __STDC_VERSION__ < 201112L
    #define static_assert(expr, msg) assert((expr) && (msg))
#endif

#endif
