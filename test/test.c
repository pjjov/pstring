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

#include "pf_assert.h"
#include "pf_test.h"

#include <pstring/pstring.h>

static const char *t_empty = "";
static const char *t_short = "hello";
static const char *t_str = "Hello, world!";
static const char *t_long
    = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aenean non "
      "suscipit purus. Phasellus a malesuada odio, non pretium massa. Class "
      "aptent taciti sociosqu ad litora torquent per conubia nostra, per "
      "inceptos himenaeos. Nullam ut semper neque. Donec interdum venenatis "
      "metus, id lacinia elit. In sed gravida velit. Mauris euismod lectus in "
      "quam semper, non hendrerit tellus mollis. Suspendisse potenti. Fusce "
      "nisi nulla, vestibulum et dictum quis, scelerisque sit amet lectus. ";

static int test_pstring_new(int seed, int repetition) {
    pstring_t str = { 0 };

    pf_assert_ok(pstrnew(&str, t_empty, 0, NULL));
    pf_assert(pstrlen(&str) == 0);
    pf_assert_true(pstrsso(&str));
    pf_assert_true(pstrowned(&str));
    pstrfree(&str);

    pf_assert_ok(pstrnew(&str, t_short, 0, NULL));
    pf_assert_not_null(pstrbuf(&str));
    pf_assert(pstrlen(&str) == strlen(t_short));
    pf_assert(pstrcap(&str) >= strlen(t_short));
    pf_assert_memcmp(pstrbuf(&str), t_short, strlen(t_short) + 1);
    pf_assert_true(pstrsso(&str));
    pf_assert_true(pstrowned(&str));
    pstrfree(&str);

    pf_assert_ok(pstrnew(&str, t_str, 0, NULL));
    pf_assert_not_null(pstrbuf(&str));
    pf_assert(pstrlen(&str) == strlen(t_str));
    pf_assert(pstrcap(&str) >= strlen(t_str));
    pf_assert_memcmp(pstrbuf(&str), t_str, strlen(t_str) + 1);
    pf_assert_true(pstrsso(&str));
    pf_assert_true(pstrowned(&str));
    pstrfree(&str);

    pf_assert_ok(pstrnew(&str, t_long, 0, NULL));
    pf_assert_not_null(pstrbuf(&str));
    pf_assert(pstrlen(&str) == strlen(t_long));
    pf_assert(pstrcap(&str) >= strlen(t_long));
    pf_assert_memcmp(pstrbuf(&str), t_long, strlen(t_long) + 1);
    pf_assert_false(pstrsso(&str));
    pf_assert_true(pstrowned(&str));
    pstrfree(&str);

    return 0;
}

static int test_pstring_alloc(int seed, int repetition) {
    pstring_t str = { 0 };

    pf_assert_ok(pstralloc(&str, 10, NULL));
    pf_assert_not_null(pstrbuf(&str));
    pf_assert(pstrlen(&str) == 0);
    pf_assert(pstrcap(&str) >= 10);
    pf_assert_true(pstrsso(&str));
    pf_assert_true(pstrowned(&str));
    pstrfree(&str);

    return 0;
}

static int test_pstring_wrap_slice(int seed, int repetition) {
    pstring_t str = { 0 };
    pstring_t slice = { 0 };
    char buffer[1024] = "Hello, world!";

    pf_assert_ok(pstrwrap(&str, buffer, strlen(buffer), 1024));
    pf_assert(pstrlen(&str) == strlen(buffer));
    pf_assert(pstrcap(&str) == 1024);
    pf_assert(pstrbuf(&str) == buffer);
    pf_assert_false(pstrsso(&str));
    pf_assert_false(pstrowned(&str));
    pf_assert(PSTRING_EINVAL == pstrgrow(&str, 1));
    pf_assert(PSTRING_EINVAL == pstrshrink(&str));

    pf_assert_ok(pstrslice(&slice, &str, 7, 12));
    pf_assert(pstrlen(&slice) == 5);
    pf_assert(pstrcap(&slice) == 5);
    pf_assert_not_null(pstrbuf(&slice));
    pf_assert_memcmp(pstrbuf(&slice), "world", 5);
    pf_assert_false(pstrsso(&str));
    pf_assert_false(pstrowned(&str));

    pf_assert_ok(pstrrange(&slice, &str, &buffer[7], &buffer[12]));
    pf_assert(pstrlen(&slice) == 5);
    pf_assert(pstrcap(&slice) == 5);
    pf_assert_not_null(pstrbuf(&slice));
    pf_assert_memcmp(pstrbuf(&slice), "world", 5);
    pf_assert_false(pstrsso(&str));
    pf_assert_false(pstrowned(&str));

    return 0;
}

static int test_pstring_resize(int seed, int repetition) {
    pstring_t str = { 0 };

    pf_assert_ok(pstralloc(&str, 4, NULL));
    pf_assert_ok(pstrgrow(&str, 7));
    pf_assert_not_null(pstrbuf(&str));
    pf_assert(pstrlen(&str) == 0);
    pf_assert(pstrcap(&str) >= 7);
    pf_assert(pstrbuf(&str)[0] == '\0');

    pf_assert_ok(pstrreserve(&str, 1));
    pf_assert_not_null(pstrbuf(&str));
    pf_assert(pstrlen(&str) == 0);
    pf_assert(pstrcap(&str) >= 8);
    pf_assert_false(pstrsso(&str));

    pf_assert_ok(pstrreserve(&str, 32));
    pf_assert_not_null(pstrbuf(&str));
    pf_assert(pstrlen(&str) == 0);
    pf_assert(pstrcap(&str) >= 32);
    pf_assert_false(pstrsso(&str));

    pf_assert_ok(pstrshrink(&str));
    pf_assert(pstrlen(&str) == 0);

    pstrfree(&str);
    return 0;
}

static int test_pstring_compare(int seed, int repetitition) {
    pstring_t a = { 0 }, b = { 0 };

    pf_assert_ok(pstrwrap(&a, "Hello, world!", 0, 0));
    pf_assert_ok(pstrwrap(&b, "Hello, world!\0", 0, 0));
    pf_assert_true(pstrequal(&a, &b));
    pf_assert(0 == pstrcmp(&a, &b));

    pf_assert_ok(pstrwrap(&a, "foo", 0, 0));
    pf_assert_ok(pstrwrap(&b, "fo0", 0, 0));
    pf_assert_false(pstrequal(&a, &b));
    pf_assert(0 < pstrcmp(&a, &b));

    pf_assert_ok(pstrwrap(&a, "bar", 0, 0));
    pf_assert_ok(pstrwrap(&b, "foo", 0, 0));
    pf_assert_false(pstrequal(&a, &b));
    pf_assert(0 > pstrcmp(&a, &b));

    pf_assert_ok(pstrwrap(&a, "", 0, 0));
    pf_assert_ok(pstrwrap(&b, "\0", 0, 0));
    pf_assert_true(pstrequal(&a, &b));
    pf_assert(0 == pstrcmp(&a, &b));

    return 0;
}

static int test_pstring_concat(int seed, int repetition) {
    pstring_t a = { 0 }, b = { 0 };
    pf_assert_ok(pstralloc(&a, 32, NULL));

    pf_assert_ok(pstrwrap(&b, "Hello", 0, 0));
    pf_assert_ok(pstrcat(&a, &b));
    pf_assert(pstrlen(&a) == 5);
    pf_assert_memcmp(pstrbuf(&a), "Hello", 5);
    pf_assert(*pstrend(&a) == '\0');

    pf_assert_ok(pstrwrap(&b, ", ", 0, 0));
    pf_assert_ok(pstrcat(&a, &b));
    pf_assert(pstrlen(&a) == 7);
    pf_assert_memcmp(pstrbuf(&a), "Hello, ", 7);
    pf_assert(*pstrend(&a) == '\0');

    pf_assert_ok(pstrwrap(&b, "world", 0, 0));
    pf_assert_ok(pstrcat(&a, &b));
    pf_assert(pstrlen(&a) == 12);
    pf_assert_memcmp(pstrbuf(&a), "Hello, world", 12);
    pf_assert(*pstrend(&a) == '\0');

    pf_assert_ok(pstrwrap(&b, "!", 0, 0));
    pf_assert_ok(pstrcat(&a, &b));
    pf_assert(pstrlen(&a) == 13);
    pf_assert_memcmp(pstrbuf(&a), "Hello, world!", 13);
    pf_assert(*pstrend(&a) == '\0');

    pf_assert_ok(pstrwrap(&b, "", 0, 0));
    pf_assert_ok(pstrcat(&a, &b));
    pf_assert(pstrlen(&a) == 13);
    pf_assert_memcmp(pstrbuf(&a), "Hello, world!", 13);
    pf_assert(*pstrend(&a) == '\0');

    pstrfree(&a);
    return 0;
}

static int test_pstring_join(int seed, int repetition) {
    pstring_t src[5] = {
        PSTRWRAP("Hello"), PSTRWRAP(", "), PSTRWRAP("world"),
        PSTRWRAP(""),      PSTRWRAP("!"),
    };

    pstring_t out = { 0 };
    pf_assert_ok(pstralloc(&out, 32, NULL));

    pf_assert_ok(pstrjoin(&out, src, 5));
    pf_assert(pstrlen(&out) == 13);
    pf_assert_memcmp(pstrbuf(&out), "Hello, world!", 13);
    pf_assert(*pstrend(&out) == '\0');

    pstrfree(&out);
    return 0;
}

static int test_pstring_copy(int seed, int repetition) {
    pstring_t dst = { 0 }, src = PSTRWRAP("Hello, world!");
    pf_assert_ok(pstralloc(&dst, pstrlen(&src), NULL));

    pf_assert_ok(pstrcpy(&dst, &src));
    pf_assert(pstrlen(&dst) == pstrlen(&src));
    pf_assert_memcmp(pstrbuf(&dst), "Hello, world!", 13);
    pf_assert(*pstrend(&dst) == '\0');

    pstrfree(&dst);
    return 0;
}

static int test_pstring_chr(int seed, int repetition) {
    char str[] = "foo foo bar buzz";
    pstring_t pstr = PSTRWRAP(str);

    pf_assert(&str[0] == pstrchr(&pstr, 'f'));
    pf_assert(&str[1] == pstrchr(&pstr, 'o'));
    pf_assert(&str[8] == pstrchr(&pstr, 'b'));
    pf_assert(&str[14] == pstrchr(&pstr, 'z'));

    pf_assert(&str[4] == pstrrchr(&pstr, 'f'));
    pf_assert(&str[6] == pstrrchr(&pstr, 'o'));
    pf_assert(&str[12] == pstrrchr(&pstr, 'b'));
    pf_assert(&str[15] == pstrrchr(&pstr, 'z'));

    pf_assert_null(pstrchr(&pstr, 'A'));
    pf_assert_null(pstrrchr(&pstr, 'A'));

    return 0;
}

static int test_pstring_span(int seed, int repetition) {
    pstring_t str = PSTRWRAP("AbccDef%$a3145bcb");

    pf_assert(1 == pstrspn(&str, "AD%5"));
    pf_assert(4 == pstrspn(&str, "Abc"));
    pf_assert(0 == pstrspn(&str, "%$"));
    pf_assert(0 == pstrspn(&str, " "));
    pf_assert(0 == pstrspn(&str, "\0"));
    pf_assert(0 == pstrspn(NULL, NULL));

    pf_assert(0 == pstrcspn(&str, "AD%5"));
    pf_assert(0 == pstrcspn(&str, "Abc"));
    pf_assert(7 == pstrcspn(&str, "%$"));
    pf_assert(17 == pstrcspn(&str, " "));
    pf_assert(17 == pstrcspn(&str, "\0"));
    pf_assert(0 == pstrcspn(NULL, NULL));

    pf_assert(0 == pstrrspn(&str, "AD%5"));
    pf_assert(3 == pstrrspn(&str, "Abc"));
    pf_assert(0 == pstrrspn(&str, "%$"));
    pf_assert(0 == pstrrspn(&str, " "));
    pf_assert(0 == pstrrspn(&str, "\0"));
    pf_assert(0 == pstrrspn(NULL, NULL));

    pf_assert(3 == pstrrcspn(&str, "AD%5"));
    pf_assert(0 == pstrrcspn(&str, "Abc"));
    pf_assert(8 == pstrrcspn(&str, "%$"));
    pf_assert(17 == pstrrcspn(&str, " "));
    pf_assert(17 == pstrrcspn(&str, "\0"));
    pf_assert(0 == pstrrcspn(NULL, NULL));

    return 0;
}

int test_pstring_breakset(int seed, int repetition) {
    pstring_t str = PSTRWRAP("AbccDef%$a3145bcb");

    pf_assert(&pstrbuf(&str)[7] == pstrpbrk(&str, "%$"));
    pf_assert(&pstrbuf(&str)[10] == pstrpbrk(&str, "12345"));
    pf_assert(&pstrbuf(&str)[7] == pstrpbrk(&str, "%$"));
    pf_assert(NULL == pstrpbrk(&str, " "));
    pf_assert(NULL == pstrpbrk(&str, "\0"));
    pf_assert(NULL == pstrpbrk(NULL, NULL));

    return 0;
}

int test_pstring_substring(int seed, int repetition) {
    pstring_t str = PSTRWRAP("Hello, world!");

    pf_assert(pstrbuf(&str) == pstrstr(&str, &PSTRWRAP("")));
    pf_assert(pstrbuf(&str) == pstrstr(&str, &PSTRWRAP("Hello")));
    pf_assert(&pstrbuf(&str)[7] == pstrstr(&str, &PSTRWRAP("world")));
    pf_assert(&pstrbuf(&str)[12] == pstrstr(&str, &PSTRWRAP("!")));
    pf_assert_null(pstrstr(&str, &PSTRWRAP("\0")));
    pf_assert_null(pstrstr(&str, &PSTRWRAP("hello")));
    pf_assert_null(pstrstr(NULL, NULL));

    return 0;
}

static const struct pf_test suite[] = {
    { test_pstring_new, "/pstring/new", 1 },
    { test_pstring_alloc, "/pstring/alloc", 1 },
    { test_pstring_wrap_slice, "/pstring/wrap_slice", 1 },
    { test_pstring_resize, "/pstring/resize", 1 },
    { test_pstring_compare, "/pstring/compare", 1 },
    { test_pstring_concat, "/pstring/concat", 1 },
    { test_pstring_join, "/pstring/join", 1 },
    { test_pstring_copy, "/pstring/copy", 1 },
    { test_pstring_chr, "/pstring/chr", 1 },
    { test_pstring_span, "/pstring/span", 1 },
    { test_pstring_breakset, "/pstring/breakset", 1 },
    { test_pstring_substring, "/pstring/substring", 1 },
    { 0 },
};

int main() { return pf_suite_run(suite, 0); }
