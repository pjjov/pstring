/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pf_assert.h>
#include <pf_test.h>

#include <pstring/object.h>
#include <pstring/pstring.h>

#include <string.h>

int test_object_scalars(int seed, int rep) {
    pstrobj_t *obj = pstrobj_new(NULL);
    int status;

    pf_assert_not_null(obj);
    pf_assert(obj->type == PSTROBJ_NULL);

    pf_assert_ok(pstrobj_set_bool(obj, 1));
    pf_assert(obj->type == PSTROBJ_BOOL);
    status = 0;
    pf_assert(1 == pstrobj_expect_bool(obj, &status));
    pf_assert(status == 0);

    pf_assert_ok(pstrobj_set_int(obj, 42));
    pf_assert(obj->type == PSTROBJ_LONG);
    status = 0;
    pf_assert(42 == pstrobj_expect_long(obj, &status));
    pf_assert(status == 0);
    status = 0;
    pf_assert(42 == pstrobj_expect_int(obj, &status));

    pf_assert_ok(pstrobj_set_double(obj, 3.5));
    pf_assert(obj->type == PSTROBJ_DOUBLE);
    status = 0;
    pf_assert(3.5 == pstrobj_expect_double(obj, &status));
    pf_assert(status == 0);

    /* long can be read back as double, but not vice versa */
    pf_assert_ok(pstrobj_set_long(obj, 7));
    status = 0;
    pf_assert(7.0 == pstrobj_expect_double(obj, &status));
    pf_assert(status == 0);

    pf_assert_ok(pstrobj_set_null(obj));
    pf_assert(obj->type == PSTROBJ_NULL);
    status = 0;
    pstrobj_expect_null(obj, &status);
    pf_assert(status == 0);

    /* type mismatch reports an error */
    pf_assert_ok(pstrobj_set_int(obj, 1));
    status = 0;
    pstrobj_expect_bool(obj, &status);
    pf_assert(status != 0);

    pstrobj_free(obj);
    return 0;
}

int test_object_defaults(int seed, int rep) {
    pstrobj_t *obj = pstrobj_new(NULL);

    pf_assert(9 == pstrobj_get_int(obj, 9));
    pf_assert(9 == pstrobj_get_long(obj, 9));

    pstrobj_set_int(obj, 5);
    pf_assert(5 == pstrobj_get_int(obj, 9));

    pstrobj_free(obj);
    return 0;
}

int test_object_string(int seed, int rep) {
    pstrobj_t *obj = pstrobj_new(NULL);
    int status;

    pf_assert_ok(pstrobj_copy_string(obj, "hello", 5));
    pf_assert(obj->type == PSTROBJ_STRING);
    status = 0;
    pf_assert(0 == strcmp("hello", pstrobj_expect_string(obj, &status)));
    pf_assert(status == 0);

    pf_assert_ok(pstrobj_copy_pstring(obj, PSTR("world")));
    status = 0;
    pf_assert(0 == strcmp("world", pstrobj_expect_string(obj, &status)));

    pstrobj_free(obj);
    return 0;
}

int test_object_list(int seed, int rep) {
    pstrobj_t *list = pstrobj_new(NULL);
    pf_assert_ok(pstrobj_set_list(list));

    pstrobj_t *a = pstrobj_new(NULL);
    pstrobj_set_int(a, 1);
    pstrobj_t *b = pstrobj_new(NULL);
    pstrobj_set_int(b, 2);
    pstrobj_t *c = pstrobj_new(NULL);
    pstrobj_set_int(c, 3);

    pf_assert_ok(pstrobj_list_insert(list, a, 0));
    pf_assert_ok(pstrobj_list_insert(list, c, 1));
    pf_assert_ok(pstrobj_list_insert(list, b, 1));

    int status = 0;
    int i = 0;
    pstrobj_t *child;
    long expected[] = { 1, 2, 3 };
    PSTROBJ_FOREACH(list, child) {
        pf_assert(expected[i++] == pstrobj_expect_long(child, &status));
    }
    pf_assert(i == 3);

    pstrobj_t *removed = pstrobj_list_remove(list, 1);
    pf_assert_not_null(removed);
    status = 0;
    pf_assert(2 == pstrobj_expect_long(removed, &status));
    pstrobj_free(removed);

    i = 0;
    long expected2[] = { 1, 3 };
    PSTROBJ_FOREACH(list, child) {
        pf_assert(expected2[i++] == pstrobj_expect_long(child, &status));
    }
    pf_assert(i == 2);

    pstrobj_free(list);
    return 0;
}

int test_object_dict(int seed, int rep) {
    pstrobj_t *dict = pstrobj_new(NULL);
    pf_assert_ok(pstrobj_set_dict(dict));

    pstrobj_t *x = pstrobj_new(NULL);
    pstrobj_copy_key(x, PSTR("x"));
    pstrobj_set_int(x, 1);

    pstrobj_t *y = pstrobj_new(NULL);
    pstrobj_copy_key(y, PSTR("y"));
    pstrobj_set_int(y, 2);

    pf_assert_ok(pstrobj_dict_insert(dict, x));
    pf_assert_ok(pstrobj_dict_insert(dict, y));

    pf_assert(PSTRING_EEXIST == pstrobj_dict_insert(dict, y));

    pstrobj_t *found = pstrobj_dict_gets(dict, "x", 1);
    pf_assert_not_null(found);
    int status = 0;
    pf_assert(1 == pstrobj_expect_long(found, &status));

    pf_assert_null(pstrobj_dict_gets(dict, "z", 1));

    pstrobj_t *removed = pstrobj_dict_remove(dict, "y", 1);
    pf_assert_not_null(removed);
    pstrobj_free(removed);
    pf_assert_null(pstrobj_dict_gets(dict, "y", 1));

    pstrobj_free(dict);
    return 0;
}

int test_object_query(int seed, int rep) {
    pstring_t src = PSTRWRAP(
        "{\"a\":1,\"b\":{\"c\":[10,20,30]},\"d\":\"hello\"}"
    );
    pstrobj_t *obj = pstrobj_from_buffer("json", &src, NULL);
    pf_assert_not_null(obj);

    int status = 0;
    pf_assert(1 == pstrobj_query_long(obj, "/a", &status));
    pf_assert(status == 0);

    status = 0;
    pf_assert(20 == pstrobj_query_long(obj, "/b/c/1", &status));
    pf_assert(status == 0);

    status = 0;
    pf_assert(30 == pstrobj_query_long(obj, "/b/c/-", &status));
    pf_assert(status == 0);

    status = 0;
    const char *s = pstrobj_query_string(obj, "/d", &status);
    pf_assert(status == 0);
    pf_assert(0 == strcmp(s, "hello"));

    pf_assert_null(pstrobj_query(obj, "/nonexistent"));

    pstrobj_free(obj);
    return 0;
}

int test_object_json_roundtrip(int seed, int rep) {
    pstring_t src = PSTRWRAP(
        "{\"i\":13,\"f\":2.5,\"s\":\"hi\",\"b\":true,\"n\":null,"
        "\"l\":[1,2,3]}"
    );
    pstrobj_t *obj = pstrobj_from_buffer("json", &src, NULL);
    pf_assert_not_null(obj);

    pstring_t out = { 0 };
    pf_assert_ok(pstrobj_to_buffer(obj, "json", &out));
    pf_assert(pstrlen(&out) > 0);

    pstrobj_t *reparsed = pstrobj_from_buffer("json", &out, NULL);
    pf_assert_not_null(reparsed);

    int status = 0;
    pf_assert(13 == pstrobj_query_long(reparsed, "/i", &status));
    status = 0;
    pf_assert(1 == pstrobj_query_bool(reparsed, "/b", &status));

    pstrfree(&out);
    pstrobj_free(obj);
    pstrobj_free(reparsed);
    return 0;
}

int test_object_xml_roundtrip(int seed, int rep) {
    pstring_t src = PSTRWRAP("{\"name\":\"Ann\",\"age\":30}");
    pstrobj_t *obj = pstrobj_from_buffer("json", &src, NULL);
    pf_assert_not_null(obj);

    pstring_t xml = { 0 };
    pf_assert_ok(pstrobj_to_buffer(obj, "xml", &xml));
    pf_assert(pstrlen(&xml) > 0);
    pf_assert_not_null(pstrstr(&xml, &PSTRWRAP("name")));
    pf_assert_not_null(pstrstr(&xml, &PSTRWRAP("Ann")));

    pstrobj_t *reparsed = pstrobj_from_buffer("xml", &xml, NULL);
    pf_assert_not_null(reparsed);

    int status = 0;
    const char *name = pstrobj_query_string(reparsed, "/name", &status);
    pf_assert(status == 0);
    pf_assert(0 == strcmp(name, "Ann"));

    pstrfree(&xml);
    pstrobj_free(obj);
    pstrobj_free(reparsed);
    return 0;
}

int test_object_errors(int seed, int rep) {
    pf_assert(PSTRTHROW_EINVAL == pstrobj_set_bool(NULL, 1));
    pf_assert(PSTRTHROW_EINVAL == pstrobj_list_insert(NULL, NULL, 0));
    pf_assert_null(pstrobj_list_remove(NULL, 0));
    pf_assert(PSTRTHROW_EINVAL == pstrobj_dict_insert(NULL, NULL));
    pf_assert_null(pstrobj_dict_get(NULL, NULL));

    pstrobj_t *not_a_dict = pstrobj_new(NULL);
    pstrobj_set_int(not_a_dict, 1);
    pstrobj_t *item = pstrobj_new(NULL);
    pf_assert(PSTRTHROW_EOBJECT == pstrobj_list_insert(not_a_dict, item, 0));

    pstrobj_free(item);
    pstrobj_free(not_a_dict);
    return 0;
}

const pf_test_t suite_object[] = {
    { test_object_scalars, "/pstring/object/scalars" },
    { test_object_defaults, "/pstring/object/defaults" },
    { test_object_string, "/pstring/object/string" },
    { test_object_list, "/pstring/object/list" },
    { test_object_dict, "/pstring/object/dict" },
    { test_object_query, "/pstring/object/query" },
    { test_object_json_roundtrip, "/pstring/object/json_roundtrip" },
    { test_object_xml_roundtrip, "/pstring/object/xml_roundtrip" },
    { test_object_errors, "/pstring/object/errors" },
    { 0 },
};
