/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pf_test.h>

/* clang-format off */

extern const pf_test_t suite_pstring[];
extern const pf_test_t suite_encoding[];
extern const pf_test_t suite_io[];
extern const pf_test_t suite_eval[];
extern const pf_test_t suite_glob[];
extern const pf_test_t suite_object[];
extern const pf_test_t suite_wordexp[];

/* clang-format on */

int main(int argc, char *argv[]) {
    static const pf_suite_t suites[] = {
        { "core", 1, suite_pstring },    { "encoding", 1, suite_encoding },
        { "io", 1, suite_io },           { "eval", 1, suite_eval },
        { "glob", 1, suite_glob },       { "object", 1, suite_object },
        { "wordexp", 1, suite_wordexp }, { 0 },
    };

    return pf_test_main(argc, argv, suites);
}
