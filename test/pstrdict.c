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

#include <pf_assert.h>
#include <pf_test.h>

#include <pstring/pstring.h>
#include <pstring/pstrdict.h>

int test_pstrdict_new(int seed, int rep) {
    pstrdict_t *dict = pstrdict_new(NULL, NULL);
    pf_assert_not_null(dict);
    pf_assert(0 == pstrdict_count(dict));
    pf_assert(0 == pstrdict_capacity(dict));

    pstrdict_free(dict);
    return 0;
}

int test_pstrdict_reserve(int seed, int rep) {
    pstrdict_t *dict = pstrdict_new(NULL, NULL);
    pf_assert_not_null(dict);

    pf_assert_ok(pstrdict_reserve(dict, 1));
    pf_assert(1 <= pstrdict_capacity(dict));
    pf_assert_ok(pstrdict_reserve(dict, 1));
    pf_assert(1 <= pstrdict_capacity(dict));
    pf_assert_ok(pstrdict_reserve(dict, 10));
    pf_assert(10 <= pstrdict_capacity(dict));

    pstrdict_free(dict);
    return 0;
}

const struct pf_test suite_dict[] = {
    { test_pstrdict_new, "/pstring/dict/new", 1 },
    { test_pstrdict_reserve, "/pstring/dict/reserved", 1 },
    { 0 },
};
