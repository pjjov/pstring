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

#define PSTROBJ_BUFFER(x) (&pf_container_of((x), struct pstrobj_str, obj)->str)

struct pstrobj_str {
    pstrobj_t obj;
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

int pstrobj_copy_string(pstrobj_t *obj, const char *str, size_t len) {
    if (!obj || (!str && len > 0))
        return PSTRING_EINVAL;

    obj->type = PSTROBJ_STRING;
    obj->flags = PF_FLAG_CLEAR(obj->flags, PSTROBJ_FLAG_WRAP);
    obj->as.string = PSTROBJ_BUFFER(obj);
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
    obj->as.string = PSTROBJ_BUFFER(obj);
    pstrwrap(obj->as.string, (char *)str, len, 0);
    return PSTRING_OK;
}

int pstrobj_wrap_pstring(pstrobj_t *obj, const pstring_t *str) {
    if (!obj || !str)
        return PSTRING_EINVAL;
    return pstrobj_wrap_string(obj, pstrbuf(str), pstrlen(str));
}
