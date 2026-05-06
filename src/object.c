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

#include <pstring/io.h>
#include <pstring/object.h>
#include <pstring/pstring.h>

#include <allocator.h>
#include <allocator_std.h>
#include <pf_macro.h>

#define PSTROBJ_BUFFER(x, NAME)                            \
    (&pf_container_of((x), struct pstrobj_str, obj)->NAME)

struct pstrobj_str {
    pstrobj_t obj;
    pstring_t key;
    pstring_t str;
};

pstrobj_t *pstrobj_new(allocator_t *alloc) {
    if (!alloc)
        alloc = &standard_allocator;

    pstrobj_t *obj;

    if (!(obj = allocate(alloc, sizeof(struct pstrobj_str))))
        return NULL;

    obj->next = NULL;
    obj->prev = NULL;
    obj->child = NULL;
    obj->key = NULL;
    obj->allocator = alloc;
    obj->type = PSTROBJ_NULL;
    obj->flags = PSTROBJ_FLAG_ROOT;

    return obj;
}

static void pstrobj__free(pstrobj_t *obj) {
    if (obj->child)
        pstrobj__free(obj->child);

    if (obj->next)
        pstrobj__free(obj->next);

    if (!PF_FLAG_TEST(obj->flags, PSTROBJ_FLAG_WRAP))
        pstrfree(obj->as.string);

    if (!PF_FLAG_TEST(obj->flags, PSTROBJ_FLAG_ARENA))
        deallocate(obj->allocator, obj, sizeof(struct pstrobj_str));
}

void pstrobj_free(pstrobj_t *obj) {
    if (!obj || !PF_FLAG_TEST(obj->flags, PSTROBJ_FLAG_ROOT))
        return;
    pstrobj__free(obj);
}

pstrobj_t *pstrobj_from_buffer(
    const char *format, pstring_t *source, allocator_t *allocator
) {
    if (!format || !source)
        return NULL;

    pstream_t stream;
    if (pstream_string(&stream, source))
        return NULL;

    pstream_seek(&stream, PSTR_SEEK_SET, 0);
    pstrobj_t *res = pstrobj_from_stream(format, &stream, allocator);
    pstream_close(&stream);

    return res;
}

int pstrobj_to_buffer(pstrobj_t *obj, const char *format, pstring_t *source) {
    if (!obj || !format || !source)
        return PSTRING_EINVAL;

    pstream_t stream;
    if (pstream_string(&stream, source))
        return PSTRING_EINVAL;

    int res = pstrobj_to_stream(obj, format, &stream);
    pstream_close(&stream);

    return res;
}

static struct {
    const char *name;
    pstrobj_load_fn *load;
    pstrobj_save_fn *save;
} formats[] = {
    { "json", pstrobj_load_json, pstrobj_save_json },
    { 0 },
};

static int find_format(const char *name) {
    for (int i = 0; formats[i].name; i++)
        if (0 == strcmp(name, formats[i].name))
            return i;
    return -1;
}

pstrobj_t *pstrobj_from_stream(
    const char *format, pstream_t *stream, allocator_t *allocator
) {
    if (!format || !stream)
        return NULL;

    int i = find_format(format);
    return i != -1 ? formats[i].load(stream, allocator) : NULL;
}

int pstrobj_to_stream(pstrobj_t *obj, const char *format, pstream_t *stream) {
    if (!format || !obj || !stream)
        return PSTRING_EINVAL;

    int i = find_format(format);
    return i != -1 ? formats[i].save(obj, stream) : PSTRING_ENOSYS;
}

static void free_string(pstrobj_t *obj) {
    if (obj->type == PSTROBJ_STRING
        && !PF_FLAG_TEST(obj->flags, PSTROBJ_FLAG_WRAP)) {
        pstrfree(obj->as.string);
    }
}

int pstrobj_set_null(pstrobj_t *obj) {
    if (!obj)
        return PSTRING_EINVAL;

    free_string(obj);
    obj->type = PSTROBJ_NULL;
    return PSTRING_OK;
}

int pstrobj_set_bool(pstrobj_t *obj, char value) {
    if (!obj)
        return PSTRING_EINVAL;

    free_string(obj);
    obj->type = PSTROBJ_BOOL;
    obj->as.bool_ = value ? 1 : 0;
    return PSTRING_OK;
}

int pstrobj_set_int(pstrobj_t *obj, int value) {
    return pstrobj_set_long(obj, value);
}

int pstrobj_set_long(pstrobj_t *obj, long value) {
    if (!obj)
        return PSTRING_EINVAL;

    free_string(obj);
    obj->type = PSTROBJ_LONG;
    obj->as.long_ = value;
    return PSTRING_OK;
}

int pstrobj_set_float(pstrobj_t *obj, float value) {
    return pstrobj_set_double(obj, value);
}

int pstrobj_set_double(pstrobj_t *obj, double value) {
    if (!obj)
        return PSTRING_EINVAL;

    free_string(obj);
    obj->type = PSTROBJ_DOUBLE;
    obj->as.double_ = value;
    return PSTRING_OK;
}

int pstrobj_set_list(pstrobj_t *obj) {
    if (!obj)
        return PSTRING_EINVAL;

    free_string(obj);
    obj->type = PSTROBJ_LIST;
    return PSTRING_OK;
}

int pstrobj_set_dict(pstrobj_t *obj) {
    if (!obj)
        return PSTRING_EINVAL;

    free_string(obj);
    obj->type = PSTROBJ_DICT;
    return PSTRING_OK;
}

int pstrobj_copy_string(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    pstring_t tmp;
    pstrwrap(&tmp, (char *)str, len, 0);
    return pstrobj_copy_pstring(obj, &tmp);
}

int pstrobj_copy_pstring(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return PSTRING_EINVAL;

    obj->type = PSTROBJ_STRING;
    obj->flags = PF_FLAG_CLEAR(obj->flags, PSTROBJ_FLAG_WRAP);
    obj->as.string = PSTROBJ_BUFFER(obj, str);
    return pstrdup(obj->as.string, str, obj->allocator);
}

int pstrobj_wrap_string(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    pstring_t tmp;
    pstrwrap(&tmp, (char *)str, len, 0);
    return pstrobj_copy_pstring(obj, &tmp);
}

int pstrobj_wrap_pstring(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return PSTRING_EINVAL;

    obj->type = PSTROBJ_STRING;
    obj->flags = PF_FLAG_SET(obj->flags, PSTROBJ_FLAG_WRAP);
    obj->as.string = PSTROBJ_BUFFER(obj, str);
    return pstrslice(obj->as.string, str, 0, pstrlen(str));
}

int pstrobj_copy_key(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return PSTRING_EINVAL;

    obj->type = PSTROBJ_STRING;
    obj->flags = PF_FLAG_CLEAR(obj->flags, PSTROBJ_FLAG_WRAP_KEY);
    obj->key = PSTROBJ_BUFFER(obj, key);
    return pstrdup(obj->key, str, obj->allocator);
}

int pstrobj_copy_keys(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    pstring_t tmp;
    pstrwrap(&tmp, (char *)str, len, 0);
    return pstrobj_copy_key(obj, &tmp);
}

int pstrobj_wrap_key(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return PSTRING_EINVAL;

    obj->type = PSTROBJ_STRING;
    obj->flags = PF_FLAG_SET(obj->flags, PSTROBJ_FLAG_WRAP_KEY);
    obj->key = PSTROBJ_BUFFER(obj, key);
    return pstrslice(obj->key, str, 0, pstrlen(str));
}

int pstrobj_wrap_keys(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    pstring_t tmp;
    pstrwrap(&tmp, (char *)str, len, 0);
    return pstrobj_wrap_key(obj, &tmp);
}

void pstrobj__set_string(pstrobj_t *obj, pstring_t *str) {
    if (!obj || !str)
        return;

    obj->as.string = PSTROBJ_BUFFER(obj, str);
    *obj->as.string = *str;
}

void pstrobj__set_key(pstrobj_t *obj, pstring_t *key) {
    if (!obj || !key)
        return;

    obj->key = PSTROBJ_BUFFER(obj, key);
    *obj->key = *key;
}

void pstrobj_expect_null(pstrobj_t *obj, int *status) {
    if (!status)
        return;

    *status = !obj || obj->type == PSTROBJ_NULL ? PSTRING_OK : PSTRING_EINVAL;
}

#define IMPL_EXPECT(TYPE, ENUM, NAME, MEMBER)                 \
    TYPE pstrobj_expect_##NAME(pstrobj_t *obj, int *status) { \
        if (!obj) {                                           \
            if (status)                                       \
                *status = PSTRING_EINVAL;                     \
            return 0;                                         \
        }                                                     \
        int res = obj && obj->type == ENUM;                   \
        if (status)                                           \
            *status = res;                                    \
        return res ? obj->as.MEMBER : 0;                      \
    }

#define IMPL_DEFAULT(TYPE)                                \
    TYPE pstrobj_get_##TYPE(pstrobj_t *obj, TYPE def) {   \
        int status;                                       \
        TYPE value = pstrobj_expect_##TYPE(obj, &status); \
        return status ? def : value;                      \
    }

IMPL_EXPECT(int, PSTROBJ_BOOL, bool, bool_);
IMPL_EXPECT(int, PSTROBJ_LONG, int, long_);
IMPL_EXPECT(long, PSTROBJ_LONG, long, long_);
IMPL_EXPECT(float, PSTROBJ_DOUBLE, float, double_);
IMPL_EXPECT(double, PSTROBJ_DOUBLE, double, double_);

IMPL_DEFAULT(int);
IMPL_DEFAULT(long);
IMPL_DEFAULT(float);
IMPL_DEFAULT(double);

#undef IMPL_EXPECT
#undef IMPL_DEFAULT

pstrobj_t *list_get_index(pstrobj_t *head, size_t index) {
    pstrobj_t *curr = head;
    for (size_t i = 0; i < index && curr != NULL; i++)
        curr = curr->next;
    return curr;
}

static void list_remove_item(pstrobj_t *list, pstrobj_t *node) {
    pstrobj_t *prev = node->prev;
    pstrobj_t *next = node->next;

    if (prev)
        prev->next = next;
    if (next)
        next->prev = prev;
    if (node == list->child)
        list->child = next;
}

static void list_insert_item(
    pstrobj_t *list, pstrobj_t *prev, pstrobj_t *item
) {
    pstrobj_t *next = prev->next;
    item->prev = prev;
    item->next = next;
    prev->next = item;
    if (next)
        next->prev = item;
}

int pstrobj_list_insert(pstrobj_t *list, pstrobj_t *item, size_t i) {
    if (!list || !item)
        return PSTRING_EINVAL;
    if (list->type != PSTROBJ_LIST && list->type != PSTROBJ_DICT)
        return PSTRING_EINVAL;
    if (item->next || item->prev)
        return PSTRING_EINVAL;

    if (i == 0) {
        if (list->child)
            list->child->prev = item;

        item->next = list->child;
        list->child = item;
        return PSTRING_OK;
    }

    pstrobj_t *prev;

    if (!(prev = list_get_index(list->child, i - 1)))
        return PSTRING_EINVAL;

    list_insert_item(list, prev, item);
    return PSTRING_OK;
}

pstrobj_t *pstrobj_list_remove(pstrobj_t *list, size_t i) {
    if (!list)
        return NULL;
    if (list->type != PSTROBJ_LIST && list->type != PSTROBJ_DICT)
        return NULL;

    pstrobj_t *node;

    if (!(node = list_get_index(list->child, i)))
        return NULL;

    list_remove_item(list, node);
    return node;
}

int pstrobj_dict_insert(pstrobj_t *dict, pstrobj_t *item) {
    if (!dict || dict->type != PSTROBJ_DICT)
        return PSTRING_EINVAL;
    if (item->key == NULL)
        return PSTRING_EINVAL;

    pstrobj_t *node;
    for (node = dict->child; node; node = node->next)
        if (pstrequal(node->key, item->key))
            return PSTRING_EEXIST;

    if (dict->child)
        dict->child->prev = item;

    item->next = dict->child;
    dict->child = item;
    return PSTRING_OK;
}

pstrobj_t *pstrobj_dict_remove(
    pstrobj_t *dict, const char *key, size_t length
) {
    if (!dict || !key || dict->type != PSTROBJ_DICT)
        return NULL;

    pstrobj_t *node;
    for (node = dict->child; node; node = node->next) {
        if (pstrequals(node->key, key, length)) {
            list_remove_item(dict, node);
            break;
        }
    }

    return node;
}

pstrobj_t *pstrobj_dict_get(pstrobj_t *dict, const pstring_t *key) {
    if (!dict || !key)
        return NULL;

    pstrobj_t *node;
    for (node = dict->child; node; node = node->next)
        if (pstrequal(node->key, key))
            return node;

    return NULL;
}

pstrobj_t *pstrobj_dict_gets(pstrobj_t *dict, const char *key, size_t length) {
    pstring_t tmp;
    pstrwrap(&tmp, (char *)key, length, 0);
    return pstrobj_dict_get(dict, &tmp);
}

static const pstring_t *query_escape(pstring_t *dst, pstring_t *src) {
    char *esc;

    if (!(esc = pstrchr(src, '~')))
        return src;

    pstrclear(dst);

    do {
        if (esc + 1 == pstrend(src))
            return NULL;

        pstrcats(dst, pstrbuf(src), esc - pstrbuf(src));

        switch (esc[1]) {
        case '0':
            pstrcatc(dst, '~');
            break;
        case '1':
            pstrcatc(dst, '/');
            break;
        default:
            return NULL;
        }

        pstrrshift(src, esc - pstrbuf(src) + 2);
    } while ((esc = pstrchr(src, '~')));

    pstrcat(dst, src);
    return dst;
}

static pstrobj_t *query_next(pstrobj_t *curr, const pstring_t *chunk) {
    if (curr->type == PSTROBJ_DICT)
        return pstrobj_dict_get(curr, chunk);

    if (curr->type != PSTROBJ_LIST || !curr->child)
        return NULL;

    pstrobj_t *node = curr->child;

    if (pstrequals(chunk, "-", 1)) {
        while (node->next)
            node = node->next;
        return node;
    }

    char *end;
    size_t index = strtol(pstrbuf(chunk), &end, 10);

    if (end != pstrend(chunk))
        return NULL;

    for (size_t i = 0; node && i != index; i++)
        node = node->next;

    return node;
}

pstrobj_t *pstrobj_query(pstrobj_t *obj, const char *query) {
    pstring_t search;
    pstrwrap(&search, (char *)query, 0, 0);

    if (pstrlen(&search) == 0)
        return obj;

    if (pstrget(&search, 0) != '/')
        return NULL;
    pstrrshift(&search, 1);

    pstring_t unescaped, escaped = { 0 };
    char *sep, *end = pstrend(&search);

    const pstring_t *chunk;
    pstrobj_t *curr = obj;

    do {
        sep = pstrchr(&search, '/');
        pstrrange(&unescaped, NULL, pstrbuf(&search), sep ? sep : end);
        if (!(chunk = query_escape(&escaped, &unescaped)))
            break;

        curr = query_next(curr, chunk);
        pstrrshift(&search, (sep ? sep + 1 : end) - pstrbuf(&search));
    } while (sep && curr);

    pstrfree(&escaped);
    return curr;
}

void *pstrobj_query_value(pstrobj_t *obj, const char *query) {
    pstrobj_t *res;

    if (!(res = pstrobj_query(obj, query)))
        return NULL;

    switch (res->type) {
    case PSTROBJ_NULL:
        return res;
    case PSTROBJ_BOOL:
        return &res->as.bool_;
    case PSTROBJ_LONG:
        return &res->as.long_;
    case PSTROBJ_DOUBLE:
        return &res->as.double_;
    case PSTROBJ_STRING:
        return &res->as.string;
    case PSTROBJ_LIST:
        return res->child;
    case PSTROBJ_DICT:
        return res->child;
    default:
        return NULL;
    }
}
