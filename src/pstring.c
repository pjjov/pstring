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

#if !defined(PSTRING_NO_AVX) && defined(__AVX__)
    #include <immintrin.h>
    #define PSTRING_AVX
#endif

#if !defined(PSTRING_NO_SSE) && defined(__SSE2__)
    #include <emmintrin.h>
    #define PSTRING_SSE
#endif

#ifdef PSTRING_AVX
    #define ALIGNMENT (_Alignof(__m256i))
#elif defined(PSTRING_SSE)
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

    memcpy(out->buffer, str, len);
    pstr__setlen(out, len);
    return PSTRING_OK;
}

int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc) {
    if (!out)
        return PSTRING_EINVAL;

    if (!alloc)
        alloc = &standard_allocator;

    capacity = ALIGN(capacity + 1, ALIGNMENT);
    char *buffer = allocate_aligned(alloc, capacity, ALIGNMENT);
    if (!buffer || !IS_ALIGNED((uintptr_t)buffer, ALIGNMENT)) {
        deallocate(alloc, buffer, capacity);
        return PSTRING_ENOMEM;
    }

    out->base.allocator = alloc;
    out->base.capacity = capacity - 1;
    out->base.length = 0;
    out->buffer = buffer;
    return PSTRING_OK;
}

int pstrwrap(pstring_t *out, char *buffer, size_t length, size_t capacity) {
    if (!out || !buffer)
        return PSTRING_EINVAL;

    if (length == 0) {
        if (capacity > 0)
            length = strnlen(buffer, capacity);
        else
            length = strlen(buffer);
    }

    if (capacity == 0)
        capacity = length;

    out->buffer = buffer;
    out->base.allocator = NULL;
    out->base.capacity = capacity;
    out->base.length = length;
    return PSTRING_OK;
}

int pstrdup(pstring_t *out, pstring_t *str, allocator_t *allocator) {
    if (!out || !str)
        return PSTRING_EINVAL;

    return pstrnew(out, pstrbuf(str), pstrlen(str), pstrallocator(str));
}

int pstrslice(pstring_t *out, pstring_t *str, size_t from, size_t to) {
    if (!out || !str)
        return PSTRING_EINVAL;

    if (to > pstrlen(str))
        to = pstrlen(str);
    if (from > to)
        from = to;

    return pstrwrap(out, &pstrbuf(str)[from], to - from, to - from);
}

void pstrfree(pstring_t *str) {
    if (str && pstrallocator(str)) {
        deallocate(pstrallocator(str), pstrbuf(str), pstrcap(str) + 1);
    }
}

int pstrreserve(pstring_t *str, size_t count) {
    if (!str)
        return PSTRING_EINVAL;

    if (count > 0 && pstrlen(str) + count > pstrcap(str))
        if (pstrgrow(str, GROWTH(pstrlen(str), count)))
            return PSTRING_ENOMEM;

    return PSTRING_OK;
}

int pstrgrow(pstring_t *str, size_t count) {
    if (!str || !pstrallocator(str) || count == 0)
        return PSTRING_EINVAL;

    size_t old = pstrcap(str) + 1;
    size_t capacity = ALIGN(old + count, ALIGNMENT);
    char *buffer = reallocate(pstrallocator(str), pstrbuf(str), old, capacity);

    if (!buffer)
        return PSTRING_ENOMEM;

    str->buffer = buffer;
    str->base.capacity = capacity - 1;
    return PSTRING_OK;
}

int pstrshrink(pstring_t *str) {
    if (!str || !pstrallocator(str))
        return PSTRING_EINVAL;

    size_t old = pstrcap(str) + 1;
    size_t capacity = ALIGN(pstrlen(str) + 1, ALIGNMENT);
    char *buffer = reallocate(pstrallocator(str), pstrbuf(str), old, capacity);

    if (!buffer)
        return PSTRING_ENOMEM;

    str->buffer = buffer;
    str->base.capacity = capacity - 1;
    return PSTRING_OK;
}

int pstrequal(const pstring_t *left, const pstring_t *right) {
    if (left == right)
        return PSTRING_TRUE;

    size_t length = pstrlen(left);
    if (length != pstrlen(right))
        return PSTRING_FALSE;

    const char *leftBuf = pstrbuf(left);
    const char *rightBuf = pstrbuf(right);

#ifdef PSTRING_AVX
    for (; length >= 32; length -= 32) {
        __m256i leftVec = _mm256_loadu_si256((const __m256i *)leftBuf);
        __m256i rightVec = _mm256_loadu_si256((const __m256i *)rightBuf);
        if (_mm256_movemask_epi8(_mm256_cmpeq_epi8(leftVec, rightVec)))
            return PSTRING_FALSE;

        leftBuf += 32;
        rightBuf += 32;
    }
#endif

#ifdef PSTRING_SSE
    for (; length >= 16; length -= 16) {
        __m128i leftVec = _mm_loadu_si128((const __m128i *)leftBuf);
        __m128i rightVec = _mm_loadu_si128((const __m128i *)rightBuf);
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(leftVec, rightVec)))
            return PSTRING_FALSE;

        leftBuf += 16;
        rightBuf += 16;
    }
#endif

    for (int i = 0; i < length; i++)
        if (leftBuf[i] != rightBuf[i])
            return PSTRING_FALSE;

    return PSTRING_TRUE;
}

int pstrcmp(const pstring_t *left, const pstring_t *right) {
    if (left == right)
        return 0;

    size_t length = MIN(pstrlen(left), pstrlen(right));
    const char *leftBuf = pstrbuf(left);
    const char *rightBuf = pstrbuf(right);

#ifdef PSTRING_AVX
    for (; length >= 32; length -= 32) {
        __m256i leftVec = _mm256_loadu_si256((const __m256i *)leftBuf);
        __m256i rightVec = _mm256_loadu_si256((const __m256i *)rightBuf);
        int diff = _mm256_movemask_epi8(_mm256_cmpeq_epi8(leftVec, rightVec));

        if (diff == 0) {
            leftBuf += 32;
            rightBuf += 32;
        } else {
            int bit = __builtin_clz(diff);
            return leftBuf[bit] - rightBuf[bit];
        }
    }
#endif

#ifdef PSTRING_SSE
    for (; length >= 16; length -= 16) {
        __m128i leftVec = _mm_loadu_si128((const __m128i *)leftBuf);
        __m128i rightVec = _mm_loadu_si128((const __m128i *)rightBuf);
        int diff = _mm_movemask_epi8(_mm_cmpeq_epi8(leftVec, rightVec));

        if (diff == 0) {
            leftBuf += 16;
            rightBuf += 16;
        } else {
            int bit = __builtin_clz(diff);
            return leftBuf[bit] - rightBuf[bit];
        }
    }
#endif

    for (int i = 0; i < length; i++)
        if (leftBuf[i] != rightBuf[i])
            return leftBuf[i] - rightBuf[i];

    return 0;
}

int pstrcat(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (pstrlen(src) > 0) {
        if (pstrreserve(dst, pstrlen(src)))
            return PSTRING_ENOMEM;

        memcpy(&pstrbuf(dst)[pstrlen(dst)], pstrbuf(src), pstrlen(src));
        pstr__setlen(dst, pstrlen(dst) + pstrlen(src));
    }
    return PSTRING_OK;
}

int pstrcpy(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    pstr__setlen(dst, 0);
    return pstrcat(dst, src);
}

int pstrjoin(pstring_t *dst, const pstring_t *srcs, size_t count) {
    if (!dst || !srcs)
        return PSTRING_EINVAL;

    size_t req = 0;
    for (size_t i = 0; i < count; i++)
        req += pstrlen(&srcs[i]);

    if (req > 0) {
        if (pstrreserve(dst, req))
            return PSTRING_ENOMEM;

        req = pstrlen(dst);
        for (size_t i = 0; i < count; i++) {
            memcpy(&pstrbuf(dst)[req], pstrbuf(&srcs[i]), pstrlen(&srcs[i]));
            req += pstrlen(&srcs[i]);
        }

        pstr__setlen(dst, req);
    }
    return PSTRING_OK;
}

void pstr__setlen(pstring_t *str, size_t length) {
    str->base.length = length;
    str->buffer[length] = '\0';
}
