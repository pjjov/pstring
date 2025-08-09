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

#include <limits.h>
#include <pf_assert.h>
#include <pf_test.h>

#include <pstring/encoding.h>
#include <pstring/pstring.h>

int test_encoding_hex(int seed, int rep) {
    pstring_t dst = {0};
    pf_assert_ok(pstrenc_hex(&dst, &PSTRWRAP("abcdefg!")));
    pf_assert(pstrlen(&dst) == 16);
    pf_assert_memcmp(pstrbuf(&dst), "6162636465666721", 16);

    pstr__setlen(&dst, 0);
    pf_assert_ok(pstrdec_hex(&dst, &PSTRWRAP("6162636465666721")));
    pf_assert(pstrlen(&dst) == 8);
    pf_assert_memcmp(pstrbuf(&dst), "abcdefg!", 8);

    pf_assert(PSTRING_EINVAL == pstrdec_hex(&dst, &PSTRWRAP("ABCDE")));
    pf_assert(PSTRING_EINVAL == pstrenc_hex(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_hex(NULL, NULL));

    return 0;
}

const struct pf_test suite_encoding[] = {
    { test_encoding_hex, "/pstring/encoding/hex", 1 },
    { 0 },
};
