/*  pstring - String library for C with SIMD acceleration

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

#ifndef PSTRING_H
#define PSTRING_H

#include "allocator.h"
#include <stddef.h>

typedef struct pstring_t {
    allocator_t *allocator;
    size_t capacity;
    size_t length;
    char *buffer;
} pstring_t;

enum {
    PSTRING_TRUE = 1,
    PSTRING_FALSE = 0,
};

enum {
    PSTRING_OK = -0,
    PSTRING_ENOENT = -2,
    PSTRING_EINTR = -4,
    PSTRING_ENOMEM = -12,
    PSTRING_EEXIST = -17,
    PSTRING_EINVAL = -22,
    PSTRING_ENOSYS = -38,
    PSTRING_ENODATA = -61,
};

static inline char *pstrbuf(const pstring_t *str) {
    return str ? str->buffer : NULL;
}

static inline size_t pstrlen(const pstring_t *str) {
    return str ? str->length : 0;
}

static inline size_t pstrcap(const pstring_t *str) {
    return str ? str->capacity : 0;
}

int pstrnew(pstring_t *out, const char *str, size_t len, allocator_t *alloc);
int pstrwrap(pstring_t *out, char *buffer, size_t length, size_t capacity);
int pstrdup(pstring_t *out, pstring_t *str, allocator_t *allocator);
int pstrslice(pstring_t *out, pstring_t *str, size_t from, size_t to);
int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc);
void pstrfree(pstring_t *str);

int pstrreserve(pstring_t *str, size_t count);
int pstrgrow(pstring_t *str, size_t count);
int pstrshrink(pstring_t *str);

int pstrequal(const pstring_t *left, const pstring_t *right);
int pstrcmp(const pstring_t *left, const pstring_t *right);

#endif
