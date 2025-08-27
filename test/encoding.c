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

#define TEST_ENCODING(fn, from, to)                      \
    pstr__setlen(&dst, 0);                               \
    pf_assert_ok(fn(&dst, &PSTRWRAP(from)));             \
    pf_assert(pstrlen(&dst) == sizeof(to) - 1);          \
    pf_assert_memcmp(pstrbuf(&dst), to, sizeof(to) - 1);

int test_encoding_hex(int seed, int rep) {
    pstring_t dst = { 0 };

    TEST_ENCODING(pstrenc_hex, "abcdefg!", "6162636465666721");
    TEST_ENCODING(pstrdec_hex, "6162636465666721", "abcdefg!");
    TEST_ENCODING(pstrenc_hex, "", "");
    TEST_ENCODING(pstrdec_hex, "", "");

    pf_assert(PSTRING_EINVAL == pstrdec_hex(&dst, &PSTRWRAP("ABCDE")));
    pf_assert(PSTRING_EINVAL == pstrenc_hex(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_hex(NULL, NULL));

    pstrfree(&dst);
    return 0;
}

int test_encoding_url(int seed, int rep) {
    pstring_t dst = { 0 };

    TEST_ENCODING(pstrenc_url, "abcd $-hello_'", "abcd%20%24-hello_%27");
    TEST_ENCODING(pstrenc_url, "\x00\x01\x21\x7F", "%00%01%21%7F");
    TEST_ENCODING(pstrenc_url, "", "");
    TEST_ENCODING(pstrdec_url, "abcd%20%24-hello_%27", "abcd $-hello_'");
    TEST_ENCODING(pstrdec_url, "%00%01%21%7F%61", "\x00\x01\x21\x7F\x61");
    TEST_ENCODING(pstrdec_url, "abcd%20%24-hello_%27%a", "abcd $-hello_'%a");
    TEST_ENCODING(pstrdec_url, "%20", " ");
    TEST_ENCODING(pstrdec_url, "", "");

    pf_assert(PSTRING_EINVAL == pstrdec_url(&dst, &PSTRWRAP("%ZY")));
    pf_assert(PSTRING_EINVAL == pstrenc_url(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_url(NULL, NULL));

    pstrfree(&dst);
    return 0;
}

int test_encoding_base64(int seed, int rep) {
    pstring_t dst = { 0 };

    TEST_ENCODING(pstrenc_base64, "abcd $-hello_'", "YWJjZCAkLWhlbGxvXyc=");
    TEST_ENCODING(pstrdec_base64, "YWJjZCAkLWhlbGxvXyc=", "abcd $-hello_'");
    TEST_ENCODING(pstrenc_base64, "~~~", "fn5+");
    TEST_ENCODING(pstrdec_base64, "fn5+", "~~~");
    TEST_ENCODING(pstrenc_base64, "", "");
    TEST_ENCODING(pstrdec_base64, "", "");

    TEST_ENCODING(pstrenc_base64url, "abcd $-hello_'", "YWJjZCAkLWhlbGxvXyc=");
    TEST_ENCODING(pstrdec_base64url, "YWJjZCAkLWhlbGxvXyc=", "abcd $-hello_'");
    TEST_ENCODING(pstrenc_base64url, "~~~", "fn5-");
    TEST_ENCODING(pstrdec_base64url, "fn5-", "~~~");
    TEST_ENCODING(pstrenc_base64url, "", "");
    TEST_ENCODING(pstrdec_base64url, "", "");

    pf_assert(PSTRING_EINVAL == pstrenc_base64(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_base64(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrenc_base64url(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_base64url(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrenc_base64table(NULL, NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_base64table(NULL, NULL, NULL));

    pf_assert(
        PSTRING_EINVAL
        == pstrenc_base64table(&PSTRWRAP(""), &PSTRWRAP(""), &PSTRWRAP(""))
    );

    pstrfree(&dst);
    return 0;
}

int test_encoding_cstring(int seed, int rep) {
    pstring_t dst = { 0 };

    TEST_ENCODING(pstrenc_cstring, "abcd\tefg\0h\nj", "abcd\\tefg\\000h\\nj");
    TEST_ENCODING(pstrenc_cstring, "", "");

    TEST_ENCODING(pstrdec_cstring, "abcd\\tefg\\000h\\nj", "abcd\tefg\0h\nj");
    TEST_ENCODING(pstrdec_cstring, "\\xabz", "\xabz");
    TEST_ENCODING(pstrdec_cstring, "\\xab", "\xab");
    TEST_ENCODING(pstrdec_cstring, "\\xaz", "\xaz");
    TEST_ENCODING(pstrdec_cstring, "\\xa", "\xa");
    TEST_ENCODING(pstrdec_cstring, "\\u0024", "$");
    TEST_ENCODING(pstrdec_cstring, "\\u0040", "@");
    TEST_ENCODING(pstrdec_cstring, "\\u0060", "`");
    TEST_ENCODING(pstrdec_cstring, "\\u1234", "\u1234");
    TEST_ENCODING(pstrdec_cstring, "\\u12341234", "\u12341234");
    TEST_ENCODING(pstrdec_cstring, "\\U00101234", "\U00101234");
    TEST_ENCODING(pstrdec_cstring, "", "");

    pf_assert(PSTRING_EINVAL == pstrenc_cstring(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(NULL, NULL));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\xaaa")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\xz")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\x")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\u")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\U")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\u123z")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\U1234567")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\uD800")));
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\uDFFF")));
    pf_assert(
        PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\U00110000"))
    );
    pf_assert(PSTRING_EINVAL == pstrdec_cstring(&dst, &PSTRWRAP("\\u09F")));

    pstrfree(&dst);
    return 0;
}

const struct pf_test suite_encoding[] = {
    { test_encoding_hex, "/pstring/encoding/hex", 1 },
    { test_encoding_url, "/pstring/encoding/url", 1 },
    { test_encoding_base64, "/pstring/encoding/base64", 1 },
    { test_encoding_cstring, "/pstring/encoding/cstring", 1 },
    { 0 },
};
