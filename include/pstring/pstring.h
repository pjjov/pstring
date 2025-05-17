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

#ifndef PSTRING_SSO_EXTEND
    #define PSTRING_SSO_EXTEND 0
#endif

#define PSTRING_SSO_SIZE (sizeof(struct pstring_base) + PSTRING_SSO_EXTEND - 2)
#define PSTRING_BLEND(x, y, mask) ((x) ^ (((x) ^ (y)) & (mask)))

typedef struct pstring_t {
    char *buffer;
    union {
        struct pstring_base {
            size_t length;
            size_t capacity;
            allocator_t *allocator;
        } base;
        struct pstring_sso {
            char buffer[PSTRING_SSO_SIZE + 1];
            unsigned char length;
        } sso;
    };
} pstring_t;

enum {
    PSTRING_TRUE = 1,
    PSTRING_FALSE = 0,
};

enum {
    PSTRING_OK = 0,
    PSTRING_ENOENT = -2,
    PSTRING_EINTR = -4,
    PSTRING_ENOMEM = -12,
    PSTRING_EEXIST = -17,
    PSTRING_EINVAL = -22,
    PSTRING_ENOSYS = -38,
    PSTRING_ENODATA = -61,
};

static inline char *pstrbuf(const pstring_t *str) {
    return str->buffer ? str->buffer : (char *)str->sso.buffer;
}

static inline size_t pstrlen(const pstring_t *str) {
    size_t mask = (str->buffer == NULL) - 1;
    return PSTRING_BLEND(str->sso.length, str->base.length, mask);
}

static inline size_t pstrcap(const pstring_t *str) {
    size_t mask = (str->buffer == NULL) - 1;
    return PSTRING_BLEND(PSTRING_SSO_SIZE, str->base.capacity, mask);
}

static inline allocator_t *pstrallocator(const pstring_t *str) {
    return str && str->buffer ? str->base.allocator : NULL;
}

static inline int pstrsso(pstring_t *str) { return str->buffer == NULL; }

static inline int pstrowned(pstring_t *str) {
    return pstrsso(str) || pstrallocator(str) != NULL;
}

int pstrnew(pstring_t *out, const char *str, size_t len, allocator_t *alloc);
int pstrdup(pstring_t *out, pstring_t *str, allocator_t *allocator);
int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc);
void pstrfree(pstring_t *str);

#define PSTRWRAP(str)                                  \
    ((pstring_t) { .buffer = (str),                    \
                   .base.length = sizeof((str)) - 1,   \
                   .base.capacity = sizeof((str)) - 1, \
                   .base.allocator = 0 })
int pstrwrap(pstring_t *out, char *buffer, size_t length, size_t capacity);
int pstrslice(pstring_t *out, const pstring_t *str, size_t from, size_t to);

int pstrreserve(pstring_t *str, size_t count);
int pstrgrow(pstring_t *str, size_t count);
int pstrshrink(pstring_t *str);

int pstrequal(const pstring_t *left, const pstring_t *right);
int pstrcmp(const pstring_t *left, const pstring_t *right);

char *pstrchr(const pstring_t *str, int ch);
char *pstrrchr(const pstring_t *str, int ch);
char *pstrpbrk(const pstring_t *str, const char *set);
size_t pstrspn(const pstring_t *str, const char *set);
size_t pstrcspn(const pstring_t *str, const char *set);
size_t pstrrspn(const pstring_t *str, const char *set);
size_t pstrrcspn(const pstring_t *str, const char *set);

int pstrcat(pstring_t *dst, const pstring_t *src);
int pstrcpy(pstring_t *dst, const pstring_t *src);
int pstrjoin(pstring_t *dst, const pstring_t *srcs, size_t count);

static inline void pstr__setlen(pstring_t *str, size_t length) {
    if (pstrsso(str))
        str->sso.length = length;
    else
        str->base.length = length;
}

#endif
