/*
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
*/

#include <pf_test.h>
#include <string.h>

/* clang-format off */

extern const pf_test suite_pstring[];
extern const pf_test suite_dict[];
extern const pf_test suite_encoding[];
extern const pf_test suite_io[];
extern const pf_test suite_pattern[];

static const pf_test *suites[] = {
    suite_pstring,
    suite_dict,
    suite_encoding,
    suite_io,
    suite_pattern,
    NULL,
};

static const char *names[] = {
    "pstring",
    "dictionary",
    "encoding",
    "io",
    "pattern",
    NULL,
};

/* clang-format on */

int main(int argc, char *argv[]) {
    if (argc > 2) {
        fputs("usage: pstring-test [suite]\nsuites:", stderr);

        for (int i = 0; names[i]; i++) {
            fputc(' ', stderr);
            fputs(names[i], stderr);
        }

        fputc('\n', stderr);
        return -1;
    }

    if (argc == 1)
        return pf_suite_run_all(suites, 0, NULL);

    for (int i = 0; names[i]; i++) {
        if (0 == strcmp(argv[1], names[i])) {
            pf_suite_run_tap(suites[i], 0, NULL);
            return 0;
        }
    }

    fprintf(stderr, "pstring-test: unknown suite '%s'\n", argv[1]);
    return -1;
}
