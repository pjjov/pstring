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

#include <pstring/io.h>
#include <pstring/pstring.h>

#define BUF_SIZE 256

int test_io_read(int seed, int rep) {
    char buffer[BUF_SIZE];
    pstring_t *string = PSTR("Hello, world!");
    pstream_t stream;

    pf_assert_ok(pstream_string(&stream, string));
    pf_assert_ok(pstream_seek(&stream, 0, PSTR_SEEK_SET));
    pf_assert(0 == pstream_tell(&stream));

    pf_assert(5 == pstream_read(&stream, buffer, 5));
    pf_assert(5 == pstream_tell(&stream));
    pf_assert_memcmp(buffer, pstrbuf(string), 5);

    pf_assert(pstrlen(string) - 5 == pstream_read(&stream, buffer, BUF_SIZE));
    pf_assert(pstrlen(string) == pstream_tell(&stream));
    pf_assert_memcmp(buffer, &pstrbuf(string)[5], pstrlen(string) - 5);

    pf_assert_ok(pstream_seek(&stream, 0, PSTR_SEEK_SET));
    pf_assert(0 == pstream_tell(&stream));
    pf_assert(5 == pstream_read(&stream, buffer, 5));
    pf_assert(5 == pstream_tell(&stream));
    pf_assert_memcmp(buffer, pstrbuf(string), 5);

    pf_assert_ok(pstream_seek(&stream, -5, PSTR_SEEK_CUR));
    pf_assert(0 == pstream_tell(&stream));
    pf_assert_ok(pstream_seek(&stream, 0, PSTR_SEEK_END));
    pf_assert(pstrlen(string) == pstream_tell(&stream));

    pstream_close(&stream);
    return 0;
}

int test_io_write(int seed, int rep) {
    pstring_t buffer;
    pstring_t *string = &buffer;
    pstream_t stream;

    pf_assert_ok(pstrnew(&buffer, "Hello, world!", 0, NULL));
    pf_assert_ok(pstream_string(&stream, string));

    pf_assert(3 == pstream_write(&stream, "abc", 3));
    pf_assert(pstrlen(string) == pstream_tell(&stream));
    pf_assert_memcmp("abc", &pstrend(string)[-3], 3);
    pf_assert(pstrlen(string) == 16);

    pf_assert_ok(pstream_seek(&stream, 0, PSTR_SEEK_SET));
    pf_assert(0 == pstream_tell(&stream));

    pf_assert(4 == pstream_write(&stream, "ABCD", 4));
    pf_assert(4 == pstream_tell(&stream));
    pf_assert(pstrlen(string) == 16);
    pf_assert_memcmp("ABCDo, world!abc", pstrbuf(string), pstrlen(string));

    pf_assert_ok(pstream_seek(&stream, -3, PSTR_SEEK_CUR));
    pf_assert(1 == pstream_tell(&stream));
    pf_assert_ok(pstream_seek(&stream, 0, PSTR_SEEK_END));
    pf_assert(pstrlen(string) == pstream_tell(&stream));

    pstream_close(&stream);
    return 0;
}

const struct pf_test suite_io[] = {
    { test_io_read, "/pstring/io/read", 1 },
    { test_io_write, "/pstring/io/write", 1 },
    { 0 },
};
