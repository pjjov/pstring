/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/core.h>

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
    #define THREADS_STD
#else
    #ifndef _Thread_local
        #if defined(_MSC_VER)
            #define _Thread_local __declspec(thread)
        #elif defined(__GNUC__) || defined(__clang__)
            #define _Thread_local __thread
        #else
            #define NO_THREAD_LOCAL
        #endif
    #endif
#endif

#ifndef thread_local
    #define thread_local _Thread_local
#endif

#define PF_EXCEPTION_STACK_SIZE 8
#include <pf_exception.h>

#ifndef NO_THREAD_LOCAL

static thread_local pf_exception_stack_t pstring_exception_stack = { 0 };

static inline pf_exception_stack_t *get_stack(void) {
    return &pstring_exception_stack;
}

#else

static inline pf_exception_stack_t *get_stack(void) { return NULL; }

#endif

pf_exception_t *pstrcatch(int error) {
    pf_exception_stack_t *stack = get_stack();
    return pf_try_catch(stack, PSTRING_EXCEPTION, error);
}

int pstrthrow(int code, const char *func, const char *msg) {
    pf_exception_stack_t *stack = get_stack();
    return pf__throw(stack, code, 0, NULL, func, msg);
}

int pstrthrowf(int code, const char *func, const char *fmt, ...) {
    pf_exception_stack_t *stack = get_stack();

    va_list args;
    va_start(args, fmt);
    int rc = pf__vthrowf(stack, code, 0, NULL, func, fmt, args);
    va_end(args);

    return rc;
}

int pstrvthrowf(int code, const char *func, const char *fmt, va_list args) {
    pf_exception_stack_t *stack = get_stack();
    return pf__vthrowf(stack, code, 0, NULL, func, fmt, args);
}

int pstrrethrow(pf_exception_t *e) {
    pf_exception_stack_t *stack = get_stack();
    return pf_rethrow(stack, e);
}
