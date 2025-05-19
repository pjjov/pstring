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
#define PSTRING_MAX_SET 256

#ifdef PSTRING_AVX
static int
pstr__match_set_avx(const char *buffer, const char *set, size_t length) {
    __m256i vec = _mm256_loadu_si256((const __m256i *)buffer);
    int result = 0;

    #ifndef PSTRING_ALT_SPN
    __m256i tmp = _mm256_setzero_si256();
    for (size_t ch = 0; ch < length; ch++) {
        __m256i check = _mm256_set1_epi8(set[ch]);
        tmp = _mm256_or_si256(tmp, _mm256_cmpeq_epi8(vec, check));
    }
    result = _mm256_movemask_epi8(tmp);
    #else
    for (size_t ch = 0; ch < length; ch++) {
        __m256i check = _mm256_set1_epi8(set[ch]);
        result |= _mm256_movemask_epi8(_mm256_cmpeq_epi8(vec, check));
    }
    #endif

    return result;
}

static int pstr__match_chr_avx(const char *buffer, int ch) {
    __m256i vec = _mm256_set1_epi8((char)ch);
    __m256i chars = _mm256_loadu_si256((const __m256i *)buffer);
    return _mm256_movemask_epi8(_mm256_cmpeq_epi8(vec, chars));
}
#endif

#ifdef PSTRING_SSE
static int
pstr__match_set_sse(const char *buffer, const char *set, size_t length) {
    __m128i vec = _mm_loadu_si128((const __m128i *)buffer);
    int result = 0;

    #ifndef PSTRING_ALT_SPN
    __m128i tmp = _mm_setzero_si128();
    for (size_t ch = 0; ch < length; ch++) {
        __m128i check = _mm_set1_epi8(set[ch]);
        tmp = _mm_or_si128(tmp, _mm_cmpeq_epi8(vec, check));
    }
    result = _mm_movemask_epi8(tmp);
    #else
    for (size_t ch = 0; ch < length; ch++) {
        __m128i check = _mm_set1_epi8(set[ch]);
        result |= _mm_movemask_epi8(_mm_cmpeq_epi8(vec, check));
    }
    #endif

    return result;
}

static int pstr__match_chr_sse(const char *buffer, int ch) {
    __m128i vec = _mm_set1_epi8((char)ch);
    __m128i chars = _mm_loadu_si128((const __m128i *)buffer);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(vec, chars));
}
#endif

static struct {
    size_t size; /* vector size */
    int (*match_set)(const char *buffer, const char *set, size_t length);
    int (*match_chr)(const char *buffer, int ch);
} g_impl = {
#if !defined(PSTRING_DETECT) && defined(PSTRING_AVX)
    .size = 32,
    .match_set = &pstr__match_set_avx,
    .match_chr = &pstr__match_chr_avx,
#elif !defined(PSTRING_DETECT) && defined(PSTRING_SSE)
    .size = 16,
    .match_set = &pstr__match_set_sse,
    .match_chr = &pstr__match_chr_sse,
#else
    0
#endif
};

#ifdef PSTRING_DETECT
    #include "pf_cpuinfo.h"
#endif

void pstrdetect(void) {
#ifdef PSTRING_DETECT
    #ifdef PSTRING_AVX
    if (PF_HAS_AVX) {
        g_impl.size = 32;
        g_impl.match_set = &pstr__match_set_avx;
        g_impl.match_chr = &pstr__match_chr_avx;
    }
    #endif
    #ifdef PSTRING_SSE
    if (PF_HAS_SSE) {
        g_impl.size = 16;
        g_impl.match_set = &pstr__match_set_sse;
        g_impl.match_chr = &pstr__match_chr_sse;
    }
    #endif
#endif
}

static inline int pstr__ctz(int x) { return __builtin_ctz(x); }
static inline int pstr__clz(int x) { return __builtin_clz(x); }

static inline int pstr__clz2(int x, int bits) {
    return __builtin_clz(x & ((1 << bits) - 1)) - bits;
}

static inline int pstr__nlen(const char *str, size_t max) {
    return strnlen(str, max);
}

int pstrnew(pstring_t *out, const char *str, size_t len, allocator_t *alloc) {
    if (!out || !str)
        return PSTRING_EINVAL;

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
        return PSTRING_EINVAL;

    if (!alloc) {
        alloc = &standard_allocator;
        if (capacity <= PSTRING_SSO_SIZE) {
            out->buffer = NULL;
            out->sso.buffer[PSTRING_SSO_SIZE] = '\0';
            out->sso.length = 0;
            return PSTRING_OK;
        }
    }

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

int pstrslice(pstring_t *out, const pstring_t *str, size_t from, size_t to) {
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
    if (!str || count == 0 || (!pstrsso(str) && !pstrallocator(str)))
        return PSTRING_EINVAL;

    if (pstrsso(str)) {
        pstring_t tmp;
        if (pstralloc(&tmp, PSTRING_SSO_SIZE + count, NULL))
            return PSTRING_ENOMEM;

        memcpy(tmp.buffer, str->buffer, pstrlen(str));
        tmp.base.length = pstrlen(str);
        *str = tmp;
        return PSTRING_OK;
    }

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

    for (size_t i = 0; i < length; i++)
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

    for (size_t i = 0; i < length; i++)
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

char *pstrchr(const pstring_t *str, int ch) {
    if (!str)
        return NULL;

    size_t length = pstrlen(str);
    char *buffer = pstrbuf(str);
    size_t i = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            int result = g_impl.match_chr(&buffer[i], ch);

            if (result) {
                int bit = pstr__ctz(result);
                return &buffer[i + bit];
            }
        }
    }

    for (; i < length; i++)
        if (buffer[i] == (char)ch)
            return &buffer[i];

    return NULL;
}

char *pstrrchr(const pstring_t *str, int ch) {
    if (!str)
        return NULL;

    size_t length = pstrlen(str);
    char *buffer = pstrbuf(str);
    size_t i = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            char *slot = &buffer[length - i - g_impl.size];
            int result = g_impl.match_chr(slot, ch);

            if (result) {
                int bit = pstr__clz2(result, g_impl.size);
                return &slot[g_impl.size - bit - 1];
            }
        }
    }

    for (; i < length; i++)
        if (buffer[length - i - 1] == (char)ch)
            return &buffer[length - i - 1];

    return NULL;
}

size_t pstrspn(const pstring_t *str, const char *set) {
    if (!str || !set)
        return 0;

    const char *buffer = pstrbuf(str);
    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, PSTRING_MAX_SET);
    size_t i = 0;
    int result = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            result = g_impl.match_set(&buffer[i], set, setlen);

            if (~result) {
                int bit = result ? pstr__ctz(~result) : 0;
                return i + bit;
            }
        }
    }

    for (result = 0; i < length; i++) {
        for (size_t j = 0; !result && j < setlen; j++)
            result |= buffer[i] == set[j];

        if (!result)
            return i;
        result = 0;
    }

    return 0;
}

size_t pstrcspn(const pstring_t *str, const char *set) {
    if (!str || !set)
        return 0;

    const char *buffer = pstrbuf(str);
    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, PSTRING_MAX_SET);
    size_t i = 0;
    int result = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            result = g_impl.match_set(&buffer[i], set, setlen);

            if (result) {
                int bit = ~result ? pstr__ctz(result) : 0;
                return i + bit;
            }
        }
    }

    for (result = 0; i < length; i++) {
        for (size_t j = 0; j < setlen; j++)
            if (buffer[i] == set[j])
                return i;
    }
    return length;
}

size_t pstrrspn(const pstring_t *str, const char *set) {
    if (!str || !set)
        return 0;

    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, PSTRING_MAX_SET);
    const char *buffer = pstrbuf(str);
    size_t i = 0;
    int result = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            const char *slot = &buffer[length - i - g_impl.size];
            result = g_impl.match_set(slot, set, setlen);

            if (~result) {
                int bit = result ? pstr__clz2(~result, g_impl.size) : 0;
                return i + bit;
            }
        }
    }

    for (result = 0; i < length; i++) {
        for (size_t j = 0; !result && j < setlen; j++)
            result |= buffer[length - i - 1] == set[j];

        if (!result)
            return i;
        result = 0;
    }

    return 0;
}

size_t pstrrcspn(const pstring_t *str, const char *set) {
    if (!str || !set)
        return 0;

    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, PSTRING_MAX_SET);
    const char *buffer = pstrbuf(str);
    size_t i = 0;
    int result = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            const char *slot = &buffer[length - i - g_impl.size];
            result = g_impl.match_set(slot, set, setlen);

            if (result) {
                int bit = ~result ? pstr__clz2(result, g_impl.size) : 0;
                return i + bit;
            }
        }
    }

    for (; i < length; i++) {
        for (size_t j = 0; j < setlen; j++)
            if (buffer[length - i - 1] == set[j])
                return i;
    }

    return length;
}

char *pstrpbrk(const pstring_t *str, const char *set) {
    if (!str || !set)
        return 0;

    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, 256);
    char *buffer = pstrbuf(str);
    size_t i = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            int result = g_impl.match_set(&buffer[i], set, setlen);

            if (result) {
                int bit = pstr__ctz(result);
                return &buffer[bit];
            }
        }
    }

    for (; i < length; i++) {
        for (size_t j = 0; j < setlen; j++)
            if (buffer[i] == set[j])
                return &buffer[i];
    }

    return NULL;
}

char *pstrstr(const pstring_t *str, const pstring_t *sub) {
    if (!str || !sub || pstrlen(sub) > pstrlen(str))
        return NULL;

    if (pstrlen(sub) == 0)
        return pstrbuf(str);

    char ch = pstrbuf(sub)[0];
    char *search = pstrbuf(str);
    char *end = &search[pstrlen(str)];
    while ((search = pstrchr(str, ch)) && search < end) {
        if (0 == memcmp(search, pstrbuf(sub), pstrlen(sub)))
            return search;
    }

    return NULL;
}
