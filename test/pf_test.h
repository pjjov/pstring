/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a minimal unit testing framework for C,
    capable of running forked tests and tracking execution time.
    The library is barebones on purpose, expecting you to bring
    your own utilities, like custom assertions with `pf_assert.h`.

    On Unix-like systems forking is used if possible, resorting to
    signal catching if not available (or if PF_TEST_NO_FORK is defined).

    Tests are run multiple times, depending on their `count`. If count
    is zero or test's callback is NULL, the test is marked as TODO.
    Current repetion index is passed to test functions, alongside a
    seed for randomization, which can be supplied by the user.

    If you want to see `pf_test.h` in action, take a look at our tests.

    The API reference:
    ```c
    typedef int (*pf_test_fn)(int seed, unsigned int i);
    typedef struct pf_test {
        pf_test_fn fn;
        const char *name;
        unsigned int count;
    } pf_test;

    int pf_test_run(const pf_test *test, int seed);
    int pf_suite_run(const pf_test *tests, int seed);
    int pf_suite_run_all(const pf_test **suites, int seed);
    ```

    You can define following configuration macros:
    - PF_TEST_NO_COLOR
    - PF_TEST_NO_FORK
    - PF_TEST_EXIT_ON_FAIL

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

#ifndef POLYFILL_TEST
#define POLYFILL_TEST

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef int (*pf_test_fn)(int seed, int i);
typedef struct pf_test {
    pf_test_fn fn; /* test function */
    const char *name; /* name of the test */
    int count; /* number of repetitions */
} pf_test;

#ifndef PF_TEST_NO_COLOR
    #define PF_TEST_TODO "\x1b[33mTODO\x1b[0m"
    #define PF_TEST_OK "\x1b[32m OK \x1b[0m"
    #define PF_TEST_FAIL "\x1b[31mFAIL\x1b[0m"
#else
    #define PF_TEST_TODO "TODO"
    #define PF_TEST_OK " OK "
    #define PF_TEST_FAIL "FAIL"
#endif

#include <signal.h>

#if !defined(PF_TEST_NO_FORK) && defined(__unix__)
    #include <sys/wait.h>
    #include <unistd.h>
#else
    #include <setjmp.h>
static volatile sig_atomic_t pf_test_signal;
static jmp_buf pf_test_jmp;
static void pf_test_signal_handler(int signal) {
    pf_test_signal = signal;
    longjmp(pf_test_jmp, 1);
}

static void pf_test_catch_signals(void) {
    signal(SIGSEGV, &pf_test_signal_handler);
    signal(SIGINT, &pf_test_signal_handler);
    signal(SIGILL, &pf_test_signal_handler);
    signal(SIGABRT, &pf_test_signal_handler);
    signal(SIGFPE, &pf_test_signal_handler);
}
#endif

static const char *pf_test_signal_format(int signal) {
    switch (signal) {
    case SIGTERM:
        return "\tSignal %d: Termination signal in %s!\n";
    case SIGSEGV:
        return "\tSignal %d: Segmentation fault occured in %s!\n";
    case SIGINT:
        return "\tSignal %d: Interrupt occured in %s!\n";
    case SIGILL:
        return "\tSignal %d: Illegal instruction in %s!\n";
    case SIGABRT:
        return "\tSignal %d: Aborted in %s!\n";
    case SIGFPE:
        return "\tSignal %d: Arithmetic exception in %s!\n";
    default:
        return "\tSignal %d: Unknown signal appeared in %s!\n";
    }
}

static void pf_test_log(const char *name, const char *level, double elapsed) {
    fprintf(stdout, "%-24s [ %s ] [ %.6fs ]\n", name, level, elapsed);
}

static void pf_test_log_signal(const char *name, int signal) {
    fprintf(stderr, pf_test_signal_format(signal), signal, name);
}

static int pf_test_exec(const pf_test *test, int seed, unsigned int i) {
#if !defined(PF_TEST_NO_FORK) && defined(__unix__)
    pid_t testPid = fork();
    if (testPid == 0)
        exit(test->fn(seed, i));
    else if (testPid == -1)
        return test->fn(seed, i);

    /* parent process */
    int status;
    pid_t newPid = waitpid(testPid, &status, 0);

    if (testPid == newPid && WIFEXITED(status)) {
        if (WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(
                stderr,
                "\tTest %s exited with %d!",
                test->name,
                WEXITSTATUS(status)
            );
            return -1;
        }
    } else {
        if (WIFSIGNALED(status))
            pf_test_log_signal(test->name, WTERMSIG(status));
        else if (WIFSTOPPED(status))
            pf_test_log_signal(test->name, WSTOPSIG(status));
        return -1;
    }

    return 0;
#else
    if (setjmp(pf_test_jmp)) {
        pf_test_log_signal(test->name, pf_test_signal);
        return -1;
    }

    return test->fn(seed, i);
#endif
}

static inline int pf_test_run(const pf_test *test, int seed) {
    if (!test || !test->name)
        return -1;

    if (!test->fn || test->count == 0) {
        pf_test_log(test->name, PF_TEST_TODO, 0);
        return -1;
    }

#if defined(PF_TEST_NO_FORK) || !defined(__unix__)
    pf_test_catch_signals();
#endif

    if (seed == 0)
        seed = time(NULL);

    long startTime = clock();
    int result = 0;
    for (unsigned int i = 0; i < test->count && !result; i++)
        result = pf_test_exec(test, seed, i);

    double elapsed = (double)(clock() - startTime) / CLOCKS_PER_SEC;
    pf_test_log(test->name, result ? PF_TEST_FAIL : PF_TEST_OK, elapsed);

#ifdef PF_TEST_EXIT_ON_FAIL
    if (result)
        exit(result);
#endif
    return result;
}

static inline int pf_suite_run(const pf_test *tests, int seed) {
    if (!tests)
        return 0;

    int passed = 0, skipped = 0, failed = 0;
    for (int i = 0; tests[i].name; i++) {
        if (!tests[i].fn || tests[i].count == 0)
            skipped++;

        if (pf_test_run(&tests[i], seed))
            failed++;
        else
            passed++;
    }

    int count = passed + skipped + failed;
    fprintf(
        stdout,
        "%u of %u (%u%%) tests passed, %u skipped. (seed: %#x)\n",
        passed,
        count,
        (passed * 100) / count,
        skipped,
        seed
    );
    return failed > 0 ? -1 : 0;
}

static inline int pf_suite_run_all(const pf_test **suites, int seed) {
    if (!suites)
        return 0;

    int passed = 0, skipped = 0, failed = 0;
    for (int s = 0; suites[s]; s++) {
        for (int i = 0; suites[s][i].name; i++) {
            if (!suites[s][i].fn || suites[s][i].count == 0)
                skipped++;

            if (pf_test_run(&suites[s][i], seed))
                failed++;
            else
                passed++;
        }
    }

    int count = passed + skipped + failed;
    fprintf(
        stdout,
        "%u of %u (%u%%) tests passed, %u skipped. (seed: %#x)\n",
        passed,
        count,
        (passed * 100) / count,
        skipped,
        seed
    );
    return failed > 0 ? -1 : 0;
}

#endif
