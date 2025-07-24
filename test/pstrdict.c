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

int test_pstrdict_get_set(int seed, int rep) {
    pstring_t keys[] = {
        PSTRWRAP("a"), PSTRWRAP("b"), PSTRWRAP("c"),
        PSTRWRAP("d"), PSTRWRAP("e"),
    };

    int values[] = { 1, 2, 3, 4, 5 };

    pstrdict_t *dict = pstrdict_new(NULL, NULL);
    pf_assert_not_null(dict);

    for (size_t i = 0; i < 5; i++)
        pf_assert_ok(pstrdict_set(dict, &keys[i], &values[i]));

    for (size_t i = 0; i < 5; i++)
        pf_assert(pstrdict_get(dict, &keys[i]) == &values[i]);

    pf_assert_null(pstrdict_get(dict, &PSTRWRAP("f")));
    pf_assert(PSTRING_EINVAL == pstrdict_set(dict, NULL, NULL));

    pstrdict_free(dict);
    return 0;
}

const struct pf_test suite_dict[] = {
    { test_pstrdict_new, "/pstring/dict/new", 1 },
    { test_pstrdict_reserve, "/pstring/dict/reserve", 1 },
    { test_pstrdict_get_set, "/pstring/dict/get_set", 1 },
    { 0 },
};
