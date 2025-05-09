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

#include <pstring/pstring.h>
#include <stdint.h>
#include <string.h>

#include "allocator_std.h"

#ifdef __SSE2__
    #include <emmintrin.h>
    #define ALIGNMENT (_Alignof(__m128i))
#else
    #define ALIGNMENT (_Alignof(char))
#endif

#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define IS_ALIGNED(x, a) (!((x) & (a - 1)))

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define GROWTH(old, req) (((old) + (req)) * 2 - (old))

int pstrnew(pstring_t *out, const char *str, size_t len, allocator_t *alloc) {
    if (!out || !str)
        return PSTRING_EINVAL;

    if (len == 0 && *str != '\0')
        len = strlen(str);

    if (pstralloc(out, len, alloc))
        return PSTRING_ENOMEM;

    out->length = len;
    out->buffer[len] = '\0';
    memcpy(out->buffer, str, len);
    return PSTRING_OK;
}

int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc) {
    if (!out || capacity == 0)
        return PSTRING_EINVAL;

    if (!alloc)
        alloc = &standard_allocator;

    capacity = ALIGN(capacity + 1, ALIGNMENT);
    char *buffer = allocate_aligned(alloc, capacity, ALIGNMENT);
    if (!buffer || !IS_ALIGNED((uintptr_t)buffer, ALIGNMENT)) {
        deallocate(alloc, buffer, capacity);
        return PSTRING_ENOMEM;
    }

    out->allocator = alloc;
    out->capacity = capacity - 1;
    out->length = 0;
    out->buffer = buffer;
    return PSTRING_OK;
}

int pstrwrap(pstring_t *out, char *buffer, size_t capacity) {
    if (!out || !buffer || capacity == 0)
        return PSTRING_EINVAL;

    out->allocator = NULL;
    out->buffer = buffer;
    out->capacity = capacity;
    out->length = 0;
    return PSTRING_OK;
}

int pstrdup(pstring_t *out, pstring_t *str, allocator_t *allocator) {
    if (!out || !str)
        return PSTRING_EINVAL;

    return pstrnew(out, pstrbuf(str), pstrlen(str), str->allocator);
}

int pstrslice(pstring_t *out, pstring_t *str, size_t from, size_t to) {
    if (!out || !str)
        return PSTRING_EINVAL;

    if (to > pstrlen(str))
        to = pstrlen(str);
    if (from > to)
        from = to;

    return pstrwrap(out, &pstrbuf(str)[from], to - from);
}

void pstrfree(pstring_t *str) {
    if (str && str->allocator) {
        deallocate(str->allocator, pstrbuf(str), pstrcap(str) + 1);
    }
}

int pstrreserve(pstring_t *str, size_t count) {
    if (!str || count == 0)
        return PSTRING_EINVAL;

    if (pstrlen(str) + count > pstrcap(str))
        if (pstrgrow(str, GROWTH(pstrlen(str), count)))
            return PSTRING_ENOMEM;

    return PSTRING_OK;
}

int pstrgrow(pstring_t *str, size_t count) {
    if (!str || !str->allocator || count == 0)
        return PSTRING_EINVAL;

    size_t old = pstrcap(str) + 1;
    size_t capacity = ALIGN(old + count, ALIGNMENT);
    char *buffer = reallocate(str->allocator, pstrbuf(str), old, capacity);

    if (!buffer)
        return PSTRING_ENOMEM;

    str->buffer = buffer;
    str->capacity = capacity - 1;
    return PSTRING_OK;
}

int pstrshrink(pstring_t *str) {
    if (!str || !str->allocator)
        return PSTRING_EINVAL;

    size_t old = pstrcap(str) + 1;
    size_t capacity = ALIGN(pstrlen(str) + 1, ALIGNMENT);
    char *buffer = reallocate(str->allocator, pstrbuf(str), old, capacity);

    if (!buffer)
        return PSTRING_ENOMEM;

    str->buffer = buffer;
    str->capacity = capacity - 1;
    return PSTRING_OK;
}
