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
        pstrobj__free(obj->child);

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

    obj->type = PSTROBJ_STRING;
    obj->flags = PF_FLAG_CLEAR(obj->flags, PSTROBJ_FLAG_WRAP);
    obj->as.string = PSTROBJ_BUFFER(obj, str);
    pstrnew(obj->as.string, str, len, obj->allocator);
    return PSTRING_OK;
}

int pstrobj_copy_pstring(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return NULL;
    return pstrobj_copy_string(obj, pstrbuf(str), pstrlen(str));
}

int pstrobj_wrap_string(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    obj->type = PSTROBJ_STRING;
    obj->flags = PF_FLAG_SET(obj->flags, PSTROBJ_FLAG_WRAP);
    obj->as.string = PSTROBJ_BUFFER(obj, str);
    pstrwrap(obj->as.string, (char *)str, len, 0);
    return PSTRING_OK;
}

int pstrobj_wrap_pstring(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return PSTRING_EINVAL;
    return pstrobj_wrap_string(obj, pstrbuf(str), pstrlen(str));
}

int pstrobj_copy_key(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return NULL;
    return pstrobj_copy_keys(obj, pstrbuf(str), pstrlen(str));
}

int pstrobj_copy_keys(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    obj->flags = PF_FLAG_CLEAR(obj->flags, PSTROBJ_FLAG_WRAP_KEY);
    obj->key = PSTROBJ_BUFFER(obj, key);
    pstrnew(obj->key, str, len, obj->allocator);
    return PSTRING_OK;
}

int pstrobj_wrap_key(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return PSTRING_EINVAL;
    return pstrobj_wrap_string(obj, pstrbuf(str), pstrlen(str));
}

int pstrobj_wrap_keys(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    obj->flags = PF_FLAG_SET(obj->flags, PSTROBJ_FLAG_WRAP_KEY);
    obj->key = PSTROBJ_BUFFER(obj, key);
    pstrwrap(obj->key, (char *)str, len, 0);
    return PSTRING_OK;
}

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

    if (!(prev = list_get_index(list->child, i)))
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
