/*  pstring - fully-featured string library for C

    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Copyright 2026 Предраг Јовановић

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <pf_test.h>

/* clang-format off */

extern const pf_test_t suite_pstring[];
extern const pf_test_t suite_dict[];
extern const pf_test_t suite_encoding[];
extern const pf_test_t suite_io[];
extern const pf_test_t suite_pattern[];
extern const pf_test_t suite_wordexp[];

/* clang-format on */

int main(int argc, char *argv[]) {
    static const pf_suite_t suites[] = {
        { "core", 1, suite_pstring },
        { "dictionary", 1, suite_dict },
        { "encoding", 1, suite_encoding },
        { "io", 1, suite_io },
        { "pattern", 1, suite_pattern },
        { "wordexp", 1, suite_wordexp },
        { 0 },
    };

    return pf_test_main(argc, argv, suites);
}
