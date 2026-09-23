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

#include <pstring/core.h>
#include <pstring/transform.h>

#include <allocator.h>
#include <allocator_std.h>
#include <pf_macro.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define ALIGNMENT 32

#define GROWTH(old, req) (((old) + (req)) * 2 - (old))

#define PSTRING_MAX_ENV_NAME 4096

size_t pstr__nlen(const char *str, size_t max) {
    if (!str)
        return 0;

    size_t i = 0;

    for (; i < max; i++)
        if (str[i] == '\0')
            break;

    return i;
}

int pstrnew(pstring_t *out, const char *str, size_t len, allocator_t *alloc) {
    if (!out || !str)
        return PSTRTHROW_EINVAL;

    if (len == 0 && *str != '\0')
        len = strlen(str);

    if (pstralloc(out, len, alloc))
        return PSTRING_ENOMEM;

    memcpy(pstrbuf(out), str, len);
    pstr__setlen(out, len);
    return PSTRING_OK;
}

int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc) {
    if (!out)
        return PSTRTHROW_EINVAL;

    if (!alloc) {
        alloc = &standard_allocator;
        if (capacity <= PSTRING_SSO_SIZE) {
            out->buffer = NULL;
            out->sso.buffer[PSTRING_SSO_SIZE] = '\0';
            out->sso.length = 0;
            return PSTRING_OK;
        }
    }

    capacity = PF_ALIGN_CEIL(capacity + 1, ALIGNMENT);
    char *buffer = allocate_aligned(alloc, capacity, ALIGNMENT);
    if (!buffer) {
        deallocate(alloc, buffer, capacity);
        return PSTRTHROW_ENOMEM;
    }

    out->base.allocator = alloc;
    out->base.capacity = capacity - 1;
    out->base.length = 0;
    out->buffer = buffer;
    return PSTRING_OK;
}

int pstrwrap(pstring_t *out, char *buffer, size_t length, size_t capacity) {
    if (!out || !buffer)
        return PSTRTHROW_EINVAL;

    if (length == 0) {
        if (capacity > 0)
            length = pstr__nlen(buffer, capacity);
        else
            length = strlen(buffer);
    }

    if (capacity == 0)
        capacity = length;

    return pstrwrapb(out, buffer, length, capacity);
}

int pstrwrapb(pstring_t *out, char *buffer, size_t length, size_t capacity) {
    if (!out || !buffer || length > capacity)
        return PSTRTHROW_EINVAL;

    out->buffer = buffer;
    out->base.allocator = NULL;
    out->base.capacity = capacity;
    out->base.length = length;
    return PSTRING_OK;
}

int pstrdup(pstring_t *out, const pstring_t *str, allocator_t *allocator) {
    if (!out || !str)
        return PSTRTHROW_EINVAL;

    pstring_t tmp;
    if (out == str) {
        tmp = *str;
        str = &tmp;
    }

    if (pstrlen(str) == 0)
        return pstralloc(out, 0, allocator);
    return pstrnew(out, pstrbuf(str), pstrlen(str), allocator);
}

char *pstrunwrap(const pstring_t *str, allocator_t *alloc) {
    if (!str)
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    if (!pstrsso(str))
        return pstrbuf(str);

    pstring_t tmp;
    int res = pstrdup(&tmp, str, alloc ? alloc : &standard_allocator);
    return res == PSTRING_OK ? pstrbuf(&tmp) : NULL;
}

int pstrslice(pstring_t *out, const pstring_t *str, size_t from, size_t to) {
    if (!out || !str)
        return PSTRTHROW_EINVAL;

    if (to > pstrlen(str))
        to = pstrlen(str);
    if (from > to)
        from = to;

    out->buffer = &pstrbuf(str)[from];
    out->base.allocator = NULL;
    out->base.capacity = to - from;
    out->base.length = to - from;
    return PSTRING_OK;
}

int pstrcut(pstring_t *str, size_t from, size_t to) {
    if (!str)
        return PSTRTHROW_EINVAL;

    if (to > pstrlen(str))
        to = pstrlen(str);
    if (from > to)
        from = to;

    if (pstrowned(str)) {
        if (to - from == 0) {
            pstrclear(str);
            return PSTRING_OK;
        }

        if (from > 0)
            memmove(pstrslot(str, 0), pstrslot(str, from), to - from);
        pstr__setlen(str, to - from);
    } else {
        pstrslice(str, str, from, to);
    }

    return PSTRING_OK;
}

int pstrrange(
    pstring_t *out, const pstring_t *str, const char *from, const char *to
) {
    if (!out)
        return PSTRTHROW_EINVAL;

    if (str) {
        if (to > pstrend(str))
            to = pstrend(str);
        if (to < pstrbuf(str))
            to = pstrbuf(str);
        if (from < pstrbuf(str))
            from = pstrbuf(str);
    }

    if (from > to)
        from = to;

    out->buffer = (char *)from;
    out->base.allocator = NULL;
    out->base.capacity = to - from;
    out->base.length = to - from;
    return PSTRING_OK;
}

void pstrfree(pstring_t *str) {
    if (str && pstrallocator(str)) {
        deallocate(pstrallocator(str), pstrbuf(str), pstrcap(str) + 1);
        *str = (pstring_t) { 0 };
    }
}

void pstrarray_free(pstrarray_t *array) {
    if (!array)
        return;

    for (size_t i = 0; i < array->length; i++)
        pstrfree(&array->items[i]);

    if (array->items && array->allocator) {
        deallocate(
            array->allocator, array->items, array->capacity * sizeof(pstring_t)
        );
    }

    *array = (pstrarray_t) { 0 };
}

int pstrdump(const pstring_t *str, char *buffer, size_t size) {
    if (!str || !buffer || size == 0)
        return PSTRING_EINVAL;
    if (pstrlen(str) >= size)
        return PSTRING_ENOMEM;

    if (pstrlen(str) != 0)
        memcpy(buffer, pstrbuf(str), pstrlen(str));
    buffer[pstrlen(str)] = '\0';
    return PSTRING_OK;
}

int pstrreserve(pstring_t *str, size_t count) {
    if (!str)
        return PSTRTHROW_EINVAL;

    if (count > 0 && pstrlen(str) + count > pstrcap(str))
        if (pstrgrow(str, GROWTH(pstrlen(str), count)))
            return PSTRING_ENOMEM;

    return PSTRING_OK;
}

int pstrgrow(pstring_t *str, size_t count) {
    if (!str || count == 0 || (!pstrsso(str) && !pstrallocator(str)))
        return PSTRTHROW_EINVAL;

    if (pstrsso(str)) {
        pstring_t tmp;
        if (pstralloc(&tmp, PSTRING_SSO_SIZE + count, NULL))
            return PSTRING_ENOMEM;

        memcpy(pstrbuf(&tmp), str->sso.buffer, PSTRING_SSO_SIZE);
        tmp.base.length = str->sso.length;
        *str = tmp;
        return PSTRING_OK;
    }

    size_t old = pstrcap(str) + 1;
    size_t capacity = PF_ALIGN_CEIL(old + count, ALIGNMENT);
    char *buffer = reallocate(pstrallocator(str), pstrbuf(str), old, capacity);

    if (!buffer)
        return PSTRTHROW_ENOMEM;

    str->buffer = buffer;
    str->base.capacity = capacity - 1;
    return PSTRING_OK;
}

int pstrshrink(pstring_t *str) {
    if (!str || !pstrallocator(str))
        return PSTRTHROW_EINVAL;

    size_t old = pstrcap(str) + 1;
    size_t capacity = PF_ALIGN_CEIL(pstrlen(str) + 1, ALIGNMENT);
    char *buffer = reallocate(pstrallocator(str), pstrbuf(str), old, capacity);

    if (!buffer)
        return PSTRTHROW_ENOMEM;

    str->buffer = buffer;
    str->base.capacity = capacity - 1;
    return PSTRING_OK;
}

int pstrequals(const pstring_t *left, const char *right, size_t length) {
    pstring_t tmp;
    pstrwrap(&tmp, (char *)right, length, length);
    return pstrequal(left, &tmp);
}

int pstrequalb(const pstring_t *left, const char *right, size_t length) {
    pstring_t tmp;
    pstrwrapb(&tmp, (char *)right, length, length);
    return pstrequal(left, &tmp);
}

int pstrcmps(const pstring_t *left, const char *right, size_t length) {
    pstring_t tmp;
    pstrwrap(&tmp, (char *)right, length, length);
    return pstrcmp(left, &tmp);
}

int pstrcpy(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    pstr__setlen(dst, 0);
    return pstrcat(dst, src);
}

static int distance(const pstring_t *left, const pstring_t *right, int **rows) {
    const char *lbuf = pstrbuf(left);
    const char *rbuf = pstrbuf(right);

#define MIN3(a, b, c) (PF_MIN(PF_MIN((a), (b)), (c)))
    int *transpose = rows[0];
    int *prev = rows[1];
    int *curr = rows[2];

    for (int j = 0; j <= pstrlen(right); j++)
        prev[j] = j;

    for (int i = 1; i <= pstrlen(left); i++) {
        curr[0] = i;

        for (int j = 1; j <= pstrlen(right); j++) {
            int cost = (lbuf[i - 1] == rbuf[j - 1]) ? 0 : 1;

            curr[j] = MIN3(curr[j - 1] + 1, prev[j] + 1, prev[j - 1] + cost);

            if (i > 1 && j > 1 && lbuf[i - 1] == rbuf[j - 2]
                && lbuf[i - 2] == rbuf[j - 1]) {
                curr[j] = PF_MIN(curr[j], transpose[j - 2] + cost);
            }
        }

        int *tmp = transpose;
        transpose = prev;
        prev = curr;
        curr = tmp;
    }

    return prev[pstrlen(right)];
}

int pstrdistance(const pstring_t *left, const pstring_t *right) {
    if (!left || !right)
        return PSTRTHROW_EINVAL;

    size_t mlen = PF_MAX(pstrlen(left), pstrlen(right)) + 1;

    if (pstrlen(left) == 0 || pstrlen(right) == 0)
        return mlen - 1;

#define PSTRDISTANCE_BUF_SIZE 1024
    int _buffer[3 * PSTRDISTANCE_BUF_SIZE];
    int *buffer = _buffer;

    size_t bytes = 3 * mlen * sizeof(int);

    if (mlen > PSTRDISTANCE_BUF_SIZE)
        buffer = allocate(&standard_allocator, bytes);

    if (buffer == NULL)
        return PSTRTHROW_ENOMEM;

    int *rows[3] = { buffer, &buffer[mlen], &buffer[2 * mlen] };
    int result = distance(left, right, rows);

    if (mlen > PSTRDISTANCE_BUF_SIZE)
        deallocate(&standard_allocator, buffer, bytes);
    return result;
}

#ifdef PSTRING_USE_XXHASH

    #include <xxhash.h>

size_t pstrhash(const pstring_t *str) {
    return XXH3_64bits(pstrbuf(str), pstrlen(str));
}

#else

size_t pstrhash(const pstring_t *str) {
    #if SIZE_MAX == 0xFFFFFFFFFFFFFFFFull
        #define HASH_FNV_PRIME 0x00000100000001b3ull
        #define HASH_FNV_OFFSET 0xcbf29ce484222325ull
    #elif SIZE_MAX == 0xFFFFFFFFull
        #define HASH_FNV_PRIME 0x01000193ull
        #define HASH_FNV_OFFSET 0x811c9dc5ull
    #endif

    size_t hash = HASH_FNV_OFFSET;
    size_t length = pstrlen(str);
    const char *bytes = pstrbuf(str);

    while (length--) {
        hash ^= (size_t)(unsigned char)*bytes++;
        hash *= HASH_FNV_PRIME;
    }

    return hash;
}

#endif

int pstrterm(pstring_t *str, allocator_t *allocator) {
    if (!str)
        return PSTRING_EINVAL;
    if (pstristerm(str))
        return PSTRING_OK;

    return pstrdup(str, str, allocator);
}

char *pstrterms(pstring_t *str, char *buffer, size_t size) {
    if (!str)
        return NULL;
    if (pstristerm(str))
        return pstrbuf(str);
    if (pstrlen(str) >= size)
        return NULL;

    if (pstrlen(str) > 0)
        memcpy(buffer, pstrbuf(str), pstrlen(str));
    buffer[pstrlen(str)] = '\0';
    return buffer;
}

int pstrenv(pstring_t *out, const pstring_t *name) {
    if (!out || !name)
        return PSTRING_EINVAL;

    char tmp[PSTRING_MAX_ENV_NAME];
    const char *cname = pstrterms((pstring_t *)name, tmp, PSTRING_MAX_ENV_NAME);

    if (!cname)
        return PSTRING_ENOMEM;

    const char *value = getenv(cname);

    if (value) {
        pstrwrap(out, (char *)value, 0, 0);
        return PSTRING_OK;
    }

    *out = (pstring_t) { 0 };
    return PSTRING_ENOENT;
}