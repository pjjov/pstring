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

#include <pstring/pstring.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <pf_bitwise.h>
#include <pf_macro.h>

#include <allocator.h>
#include <allocator_std.h>

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
static uint64_t pstr__match_set_avx(
    const char *buffer, const char *set, size_t length
) {
    __m256i vec = _mm256_loadu_si256((const __m256i *)buffer);
    uint64_t result = 0;

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

static uint64_t pstr__match_chr_avx(const char *buffer, int ch) {
    __m256i vec = _mm256_set1_epi8((char)ch);
    __m256i chars = _mm256_loadu_si256((const __m256i *)buffer);
    return _mm256_movemask_epi8(_mm256_cmpeq_epi8(vec, chars));
}

static uint64_t pstr__compare_avx(const char *left, const char *right) {
    __m256i leftVec = _mm256_loadu_si256((const __m256i *)left);
    __m256i rightVec = _mm256_loadu_si256((const __m256i *)right);
    return _mm256_movemask_epi8(_mm256_cmpeq_epi8(leftVec, rightVec));
}
#endif

#ifdef PSTRING_SSE
static uint64_t pstr__match_set_sse(
    const char *buffer, const char *set, size_t length
) {
    __m128i vec = _mm_loadu_si128((const __m128i *)buffer);
    uint64_t result = 0;

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

static uint64_t pstr__match_chr_sse(const char *buffer, int ch) {
    __m128i vec = _mm_set1_epi8((char)ch);
    __m128i chars = _mm_loadu_si128((const __m128i *)buffer);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(vec, chars));
}

static uint64_t pstr__compare_sse(const char *left, const char *right) {
    __m128i leftVec = _mm_loadu_si128((const __m128i *)left);
    __m128i rightVec = _mm_loadu_si128((const __m128i *)right);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(leftVec, rightVec));
}

#endif

static struct {
    size_t size; /* vector size */
    uint64_t (*match_set)(const char *buffer, const char *set, size_t length);
    uint64_t (*match_chr)(const char *buffer, int ch);
    uint64_t (*compare)(const char *left, const char *right);
} g_impl = {
#if !defined(PSTRING_DETECT) && defined(PSTRING_AVX)
    .size = 32,
    .match_set = &pstr__match_set_avx,
    .match_chr = &pstr__match_chr_avx,
    .compare = &pstr__compare_avx,
#elif !defined(PSTRING_DETECT) && defined(PSTRING_SSE)
    .size = 16,
    .match_set = &pstr__match_set_sse,
    .match_chr = &pstr__match_chr_sse,
    .compare = &pstr__compare_sse,
#else
    0
#endif
};

#ifdef PSTRING_DETECT
    #include <pf_cpuinfo.h>
#endif

void pstrdetect(void) {
#ifdef PSTRING_DETECT
    #ifdef PSTRING_AVX
    if (PF_HAS_AVX) {
        g_impl.size = 32;
        g_impl.match_set = &pstr__match_set_avx;
        g_impl.match_chr = &pstr__match_chr_avx;
        g_impl.compare = &pstr__compare_avx;
    }
    #endif
    #ifdef PSTRING_SSE
    if (PF_HAS_SSE) {
        g_impl.size = 16;
        g_impl.match_set = &pstr__match_set_sse;
        g_impl.match_chr = &pstr__match_chr_sse;
        g_impl.compare = &pstr__compare_sse;
    }
    #endif
#endif
}

static inline int pstr__clz_masked(uint64_t x, int bits) {
    return pf_clz(x & ((1 << bits) - 1)) - bits;
}

size_t pstr__nlen(const char *str, size_t max) {
#if __STDC_VERSION__ >= 201112L && __STDC_LIB_EXT1__
    return strnlen_s(str, max);
#elif _POSIX_C_SOURCE >= 200809L
    return strnlen(str, max);
#else
    if (!str)
        return 0;

    size_t i = 0;
    if (g_impl.size > 0) {
        for (int result; max - i >= g_impl.size; i += g_impl.size) {
            if ((result = g_impl.match_chr(str, '\0'))) {
                int bit = pf_ctz64(result);
                return i + bit;
            }
        }
    }

    for (; i < max; i++)
        if (str[i] == '\0')
            break;

    return i;
#endif
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
            length = pstr__nlen(buffer, capacity);
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

int pstrdup(pstring_t *out, const pstring_t *str, allocator_t *allocator) {
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

    out->buffer = &pstrbuf(str)[from];
    out->base.allocator = NULL;
    out->base.capacity = to - from;
    out->base.length = to - from;
    return PSTRING_OK;
}

int pstrcut(pstring_t *str, size_t from, size_t to) {
    if (!str)
        return PSTRING_EINVAL;

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
        return PSTRING_EINVAL;

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

        memcpy(pstrbuf(&tmp), str->sso.buffer, PSTRING_SSO_SIZE);
        tmp.base.length = str->sso.length;
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
    size_t i = 0, mask = (1ull << g_impl.size) - 1;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size)
            if (mask != g_impl.compare(&leftBuf[i], &rightBuf[i]))
                return PSTRING_FALSE;
    }

    for (; i < length; i++)
        if (leftBuf[i] != rightBuf[i])
            return PSTRING_FALSE;

    return PSTRING_TRUE;
}

PSTR_API int pstrequals(
    const pstring_t *left, const char *right, size_t length
) {
    pstring_t tmp;
    pstrwrap(&tmp, (char *)right, length, length);
    return pstrequal(left, &tmp);
}

int pstrcmp(const pstring_t *left, const pstring_t *right) {
    if (left == right)
        return 0;

    size_t length = MIN(pstrlen(left), pstrlen(right));
    const char *leftBuf = pstrbuf(left);
    const char *rightBuf = pstrbuf(right);
    size_t i = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            int result = ~g_impl.compare(&leftBuf[i], &rightBuf[i]);

            if (result) {
                int bit = pstr__clz_masked(result, g_impl.size);
                return leftBuf[bit] - rightBuf[bit];
            }
        }
    }

    for (; i < length; i++)
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

        memcpy(pstrend(dst), pstrbuf(src), pstrlen(src));
        pstr__setlen(dst, pstrlen(dst) + pstrlen(src));
    }

    return PSTRING_OK;
}

int pstrcats(pstring_t *dst, const char *src, size_t length) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (length == 0)
        length = strlen(src);

    if (length > 0) {
        if (pstrreserve(dst, length))
            return PSTRING_ENOMEM;

        memcpy(pstrend(dst), src, length);
        pstr__setlen(dst, pstrlen(dst) + length);
    }

    return PSTRING_OK;
}

int pstrcatc(pstring_t *dst, char chr) {
    if (!dst)
        return PSTRING_EINVAL;

    if (pstrreserve(dst, 1))
        return PSTRING_ENOMEM;

    *pstrend(dst) = chr;
    pstr__setlen(dst, pstrlen(dst) + 1);
    return PSTRING_OK;
}

int pstrrcat(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (pstrlen(src) > 0) {
        if (pstrreserve(dst, pstrlen(src)))
            return PSTRING_ENOMEM;

        memmove(pstrslot(dst, pstrlen(src)), pstrbuf(dst), pstrlen(dst));
        memcpy(pstrbuf(dst), pstrbuf(src), pstrlen(src));
        pstr__setlen(dst, pstrlen(dst) + pstrlen(src));
    }

    return PSTRING_OK;
}

static int pstr__move(pstring_t *dst, size_t at, size_t count) {
    if (pstrreserve(dst, count))
        return PSTRING_ENOMEM;

    size_t dlen = pstrlen(dst);

    if (at < dlen)
        memmove(pstrslot(dst, at + count), pstrslot(dst, at), dlen - at);

    pstr__setlen(dst, dlen + count);
    return PSTRING_OK;
}

int pstrinsert(pstring_t *dst, size_t at, pstring_t *src) {
    if (!dst || !src || at > pstrlen(dst))
        return PSTRING_EINVAL;

    if (pstrlen(src) == 0)
        return PSTRING_OK;

    if (pstr__move(dst, at, pstrlen(src)))
        return PSTRING_ENOMEM;

    memcpy(pstrslot(dst, at), pstrbuf(src), pstrlen(src));
    return PSTRING_OK;
}

int pstrinsertc(pstring_t *dst, size_t at, size_t count, char chr) {
    if (!dst || count == 0 || at > pstrlen(dst))
        return PSTRING_EINVAL;

    if (pstr__move(dst, at, count))
        return PSTRING_ENOMEM;

    memset(pstrslot(dst, at), chr, count);
    return PSTRING_OK;
}

int pstrremove(pstring_t *str, size_t from, size_t to) {
    if (!str || from >= to)
        return PSTRING_EINVAL;

    size_t len = pstrlen(str);
    if (from >= len || to > len)
        return PSTRING_EINVAL;

    if (to < len)
        memmove(pstrslot(str, from), pstrslot(str, to), len - to);
    pstr__setlen(str, len - (to - from));
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
                int bit = pf_ctz64(result);
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
                int bit = pstr__clz_masked(result, g_impl.size);
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
                int bit = result ? pf_ctz64(~result) : 0;
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
                int bit = ~result ? pf_ctz64(result) : 0;
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
                int bit = result ? pstr__clz_masked(~result, g_impl.size) : 0;
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
                int bit = ~result ? pstr__clz_masked(result, g_impl.size) : 0;
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
                int bit = pf_ctz64(result);
                return &buffer[i + bit];
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

char *pstrcpbrk(const pstring_t *str, const char *set) {
    if (!str || !set)
        return NULL;

    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, 256);
    char *buffer = pstrbuf(str);
    size_t i = 0, j;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            int result = g_impl.match_set(&buffer[i], set, setlen);
            result = ~result & ((1 << g_impl.size) - 1);

            if (result) {
                int bit = pf_ctz64(result);
                return &buffer[i + bit];
            }
        }
    }

    for (; i < length; i++) {
        for (j = 0; j < setlen; j++)
            if (buffer[i] == set[j])
                break;

        if (j == setlen)
            return &buffer[i];
    }

    return NULL;
}

char *pstrrpbrk(const pstring_t *str, const char *set) {
    if (!str || !set)
        return 0;

    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, 256);
    char *buffer = pstrbuf(str);
    size_t i = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            int result = g_impl.match_set(
                &buffer[length - i - g_impl.size], set, setlen
            );

            if (result) {
                int bit = pstr__clz_masked(result, g_impl.size);
                return &buffer[length - i - bit - 1];
            }
        }
    }

    for (; i > 0; i--) {
        for (size_t j = 0; j < setlen; j++)
            if (buffer[length - i - 1] == set[j])
                return &buffer[length - i - 1];
    }

    return NULL;
}

char *pstrrcpbrk(const pstring_t *str, const char *set) {
    if (!str || !set)
        return NULL;

    size_t length = pstrlen(str);
    size_t setlen = pstr__nlen(set, 256);
    char *buffer = pstrbuf(str);
    size_t i = 0, j;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            int result = g_impl.match_set(
                &buffer[length - i - g_impl.size], set, setlen
            );
            result = ~result & ((1 << g_impl.size) - 1);

            if (result) {
                int bit = ~result ? pstr__clz_masked(result, g_impl.size) : 0;
                return &buffer[length - i - bit - 1];
            }
        }
    }

    for (; i < length; i++) {
        for (j = 0; j < setlen; j++)
            if (buffer[length - i - 1] == set[j])
                break;

        if (j == setlen)
            return &buffer[length - i - 1];
    }

    return NULL;
}

char *pstrstr(const pstring_t *str, const pstring_t *sub) {
    if (!str || !sub || pstrlen(sub) > pstrlen(str))
        return NULL;

    if (pstrlen(sub) == 0)
        return pstrbuf(str);

    char ch = pstrbuf(sub)[0];
    char *end = pstrend(str) - pstrlen(sub) + 1;

    char *match;
    pstring_t search;
    pstrrange(&search, NULL, pstrbuf(str), end);

    while ((match = pstrchr(&search, ch))) {
        if (0 == memcmp(match, pstrbuf(sub), pstrlen(sub)))
            return match;
        pstrrange(&search, NULL, match + 1, end);
    }

    return NULL;
}

int pstrtok(pstring_t *dst, const pstring_t *src, const char *set) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (!set) {
        pstrslice(dst, src, 0, 0);
        return PSTRING_OK;
    }

    pstring_t search;

    pstrrange(&search, src, pstrend(dst), pstrend(src));
    const char *start = pstrcpbrk(&search, set);

    pstrrange(&search, src, start, pstrend(src));
    const char *end = pstrpbrk(&search, set);

    if (!start)
        return PSTRING_ENOENT;

    pstrrange(dst, src, start, end ? end : pstrend(src));
    return PSTRING_OK;
}

int pstrsplit(pstring_t *dst, const pstring_t *src, const pstring_t *sep) {
    if (!dst || !src)
        return PSTRING_EINVAL;

    if (!sep) {
        pstrslice(dst, src, 0, 0);
        return PSTRING_OK;
    }

    pstring_t search;
    const char *prev = pstrend(dst);

    pstrrange(&search, src, prev, pstrend(src));
    if (pstrprefix(&search, pstrbuf(sep), pstrlen(sep))) {
        prev += pstrlen(sep);
        pstrrange(&search, src, prev, pstrend(src));
    }

    const char *next = pstrstr(&search, sep);
    pstrrange(dst, src, prev, next ? next : pstrend(src));
    return prev == pstrend(src) ? PSTRING_ENOENT : PSTRING_OK;
}

int pstrrepl(
    pstring_t *str, const pstring_t *src, const pstring_t *dst, size_t max
) {
    if (!str || !src || !dst)
        return PSTRING_EINVAL;

    if (max == 0)
        max = SIZE_MAX;

    size_t slen = pstrlen(src);
    size_t dlen = pstrlen(dst);

    pstring_t search;
    pstrslice(&search, str, 0, pstrlen(str));
    size_t length = pstrlen(str);
    long diff = dlen - slen;
    char *match;

    while (max-- > 0) {
        if (!(match = pstrstr(&search, src)))
            break;

        if (diff > 0 && pstrreserve(str, length + diff))
            return PSTRING_ENOMEM;

        if (dlen > 0) {
            memcpy(&match[dlen], match + slen, pstrend(str) - match - slen);
            memcpy(match, pstrbuf(dst), dlen);
        } else {
            memcpy(match, &match[slen], pstrend(str) - match - slen);
        }

        length += diff;
        pstr__setlen(str, length);
        pstrrange(&search, NULL, &match[dlen], pstrend(str));
    }

    return PSTRING_OK;
}

int pstrrepls(pstring_t *str, const char *src, const char *dst, size_t max) {
    if (!src || !dst)
        return PSTRING_EINVAL;

    pstring_t _src, _dst;
    pstrwrap(&_src, (char *)src, 0, 0);
    pstrwrap(&_dst, (char *)dst, 0, 0);
    return pstrrepl(str, &_src, &_dst, max);
}

int pstrreplc(pstring_t *str, char src, char dst, size_t max) {
    if (!str || src == dst)
        return PSTRING_EINVAL;

    pstring_t search;
    char *match = pstrbuf(str);

    pstrrange(&search, NULL, match, pstrend(str));

    while ((match = pstrchr(&search, src))) {
        *match = dst;
        pstrrange(&search, NULL, match, pstrend(str));
    }

    return PSTRING_OK;
}

int pstrlstrip(pstring_t *str, const char *chars) {
    if (!str)
        return PSTRING_EINVAL;

    if (!chars)
        chars = " \t\r\n\v\f";

    char *left = pstrcpbrk(str, chars);
    if (left == NULL)
        return PSTRING_OK;

    return pstrcut(str, left - pstrbuf(str), pstrlen(str));
}

int pstrrstrip(pstring_t *str, const char *chars) {
    if (!str)
        return PSTRING_EINVAL;

    if (!chars)
        chars = " \t\r\n\v\f";

    char *right = pstrrcpbrk(str, chars);
    if (right == NULL)
        return PSTRING_OK;

    return pstrcut(str, 0, right - pstrbuf(str) + 1);
}

int pstrstrip(pstring_t *str, const char *chars) {
    if (!str)
        return PSTRING_EINVAL;

    if (!chars)
        chars = " \t\r\n\v\f";

    char *left = pstrcpbrk(str, chars);
    if (left == NULL)
        left = pstrbuf(str);

    pstring_t tmp;
    pstrrange(&tmp, NULL, left, pstrend(str));

    char *right = pstrrcpbrk(&tmp, chars);
    if (right == NULL)
        right = pstrend(str);

    return pstrcut(str, left - pstrbuf(str), (right + 1) - pstrbuf(str));
}

static int count_indent(const pstring_t *str, int max, int tab, int *out) {
    const char *chr;
    int length = 0;
    int count = 0;

    for (chr = pstrbuf(str); count < max && chr < pstrend(str); chr++) {
        if (*chr == ' ' || *chr == '\t') {
            count += *chr == '\t' ? tab : 1;
            length++;
        } else if (*chr != '\r' && *chr != '\v' && *chr != '\f') {
            break;
        }
    }

    if (out)
        *out = count;
    return length;
}

int pstrdedent(pstring_t *str, int count, int tab) {
    if (!str)
        return PSTRING_EINVAL;

    if (count <= 0)
        count = INT_MAX;
    if (tab <= 0)
        tab = 4;

    pstring_t search;
    char *prev = pstrbuf(str);
    char *end = pstrend(str);
    char *match = prev;
    char *out = prev;

    pstrrange(&search, NULL, prev, pstrend(str));

    while (match < end) {
        match = pstrchr(&search, '\n');
        if (!match)
            match = end;

        pstrrange(&search, NULL, prev, match);

        int length = count_indent(&search, count, tab, NULL);
        memmove(out, &prev[length], match - prev - length + 1);
        out += match - prev - length + 1;

        prev = match + 1;
        pstrrange(&search, NULL, prev, pstrend(str));
    }

    pstr__setlen(str, out - pstrbuf(str) - 1);
    return PSTRING_OK;
}

int pstrindent(pstring_t *str, int count, int tab) {
    if (!str)
        return PSTRING_EINVAL;

    if (count < 0)
        count = 0;
    if (tab <= 0)
        tab = 4;

    pstring_t search;
    const char *prev = pstrbuf(str);
    const char *match = prev;

    pstrrange(&search, NULL, prev, pstrend(str));
    int indent, min = -1;

    while (prev < pstrend(str)) {
        if (!(match = pstrchr(&search, '\n')))
            match = pstrend(str);

        pstrrange(&search, NULL, prev, match);

        if (count <= 0) {
            count_indent(&search, INT_MAX, tab, &indent);
            if (min == -1 || min > indent)
                min = indent;
        } else if (pstrinsertc(str, prev - pstrbuf(str), count, ' '))
            return PSTRING_ENOMEM;

        prev = match + count + 1;
        pstrrange(&search, NULL, prev, pstrend(str));
    }

    return min == -1 ? 0 : min;
}

int pstrprefix(pstring_t *str, const char *prefix, size_t length) {
    if (!str || !prefix)
        return PSTRING_EINVAL;

    if (length == 0)
        length = strlen(prefix);

    if (length > pstrlen(str))
        return PSTRING_FALSE;

    return 0 == memcmp(pstrbuf(str), prefix, length);
}

int pstrsuffix(pstring_t *str, const char *suffix, size_t length) {
    if (!str || !suffix)
        return PSTRING_EINVAL;

    if (length == 0)
        length = strlen(suffix);

    if (length > pstrlen(str))
        return PSTRING_FALSE;

    return 0 == memcmp(pstrslot(str, pstrlen(str) - length), suffix, length);
}

int pstrftime(pstring_t *dst, const char *fmt, struct tm *src) {
    if (!dst || !fmt || !src)
        return PSTRING_EINVAL;

    if (pstrreserve(dst, strlen(fmt) * 2))
        return PSTRING_ENOMEM;

    size_t space = pstrcap(dst) - pstrlen(dst);
    size_t written = strftime(pstrend(dst), space, fmt, src);
    pstr__setlen(dst, pstrlen(dst) + written);
    return written > 0 ? PSTRING_OK : PSTRING_ENOMEM;
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
                curr[j] = MIN(curr[j], transpose[j - 2] + cost);
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
        return PSTRING_EINVAL;

    size_t mlen = PF_MAX(pstrlen(left), pstrlen(right)) + 1;

    if (pstrlen(left) == 0 || pstrlen(right) == 0)
        return mlen - 1;

#define PSTRDISTANCE_BUF_SIZE 1024
    int _buffer[3 * PSTRDISTANCE_BUF_SIZE];
    int *buffer = _buffer;

    if (mlen > PSTRDISTANCE_BUF_SIZE)
        buffer = allocate(&standard_allocator, mlen * sizeof(int));

    if (buffer == NULL)
        return PSTRING_ENOMEM;

    int *rows[3] = { buffer, &buffer[mlen], &buffer[2 * mlen] };
    int result = distance(left, right, rows);

    if (mlen > PSTRDISTANCE_BUF_SIZE)
        deallocate(&standard_allocator, buffer, mlen * sizeof(int));
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
