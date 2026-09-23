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
#include <pstring/search.h>

#include <pf_bitwise.h>
#include <pf_macro.h>

#include <stdint.h>
#include <string.h>

#if !defined(PSTRING_NO_AVX) && defined(__AVX2__)
    #include <immintrin.h>
    #define PSTRING_AVX
#endif

#if !defined(PSTRING_NO_SSE) && defined(__SSE2__)
    #include <emmintrin.h>
    #define PSTRING_SSE
#endif

/* The nibble-lookup set matcher needs SSSE3's `pshufb`. Without it we still
   use SIMD for single-character and equality scans, and fall back to the
   scalar bitmap for character sets. */
#if defined(PSTRING_SSE) && !defined(PSTRING_NO_SSSE3) && defined(__SSSE3__)
    #include <tmmintrin.h>
    #define PSTRING_SSSE3
#endif

#ifdef PSTRING_AVX
    #define ALIGNMENT (_Alignof(__m256i))
#elif defined(PSTRING_SSE)
    #define ALIGNMENT (_Alignof(__m128i))
#else
    #define ALIGNMENT (_Alignof(char))
#endif

#define PSTRING_MAX_SET 256

/** A character set is compiled once per call into two representations:

    - `map`, a 256-bit bitmap used by the scalar tail, and
    - `nib`, a 16-byte table used by the vectorised `pshufb` lookup.

    For `nib`, entry `i` holds a bitmask of every high nibble `h` such that the
    byte `(h << 4) | i` is a member of the set. Testing a vector of bytes is
    then two shuffles and an AND, independent of how large the set is. The
    previous implementation broadcast and compared once per set character,
    so a 20-character set cost 20 vector compares per block. **/
typedef struct pstr_set {
    unsigned char map[32];
    unsigned char nib[16];
} pstr_set_t;

static void pstr__set_build(pstr_set_t *s, const char *set, size_t length) {
    memset(s, 0, sizeof(*s));

    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)set[i];
        s->map[c >> 3] |= (unsigned char)(1u << (c & 7));
        s->nib[c & 0x0F] |= (unsigned char)(1u << (c >> 4));
    }
}

static inline int pstr__set_test(const pstr_set_t *s, char c) {
    unsigned char u = (unsigned char)c;
    return (s->map[u >> 3] >> (u & 7)) & 1;
}

#ifdef PSTRING_AVX
static uint32_t pstr__match_chr_avx(const char *buffer, int ch) {
    __m256i vec = _mm256_set1_epi8((char)ch);
    __m256i chars = _mm256_loadu_si256((const __m256i *)buffer);
    return (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(vec, chars));
}

static uint32_t pstr__compare_avx(const char *left, const char *right) {
    __m256i leftVec = _mm256_loadu_si256((const __m256i *)left);
    __m256i rightVec = _mm256_loadu_si256((const __m256i *)right);
    return (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(leftVec, rightVec));
}

static uint32_t pstr__match_set_avx(const char *buffer, const pstr_set_t *s) {
    __m128i half = _mm_loadu_si128((const __m128i *)s->nib);
    __m256i table = _mm256_broadcastsi128_si256(half);
    __m256i bits = _mm256_setr_epi8(
        /* clang-format off */
        1, 2, 4, 8, 16, 32, 64, (char)128, 1, 2, 4, 8, 16, 32, 64, (char)128,
        1, 2, 4, 8, 16, 32, 64, (char)128, 1, 2, 4, 8, 16, 32, 64, (char)128
        /* clang-format on */
    );

    __m256i vec = _mm256_loadu_si256((const __m256i *)buffer);
    __m256i lo = _mm256_and_si256(vec, _mm256_set1_epi8(0x0F));
    __m256i hi = _mm256_and_si256(
        _mm256_srli_epi16(vec, 4), _mm256_set1_epi8(0x0F)
    );

    __m256i rows = _mm256_shuffle_epi8(table, lo);
    __m256i cols = _mm256_shuffle_epi8(bits, hi);
    __m256i test = _mm256_and_si256(rows, cols);

    /* `cols` never contains a zero byte, so `test == cols` exactly when the
       row selected by the low nibble carries the high nibble's bit. */
    return (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(test, cols));
}
#endif

#ifdef PSTRING_SSE
static uint32_t pstr__match_chr_sse(const char *buffer, int ch) {
    __m128i vec = _mm_set1_epi8((char)ch);
    __m128i chars = _mm_loadu_si128((const __m128i *)buffer);
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(vec, chars));
}

static uint32_t pstr__compare_sse(const char *left, const char *right) {
    __m128i leftVec = _mm_loadu_si128((const __m128i *)left);
    __m128i rightVec = _mm_loadu_si128((const __m128i *)right);
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(leftVec, rightVec));
}
#endif

#ifdef PSTRING_SSSE3
static uint32_t pstr__match_set_sse(const char *buffer, const pstr_set_t *s) {
    __m128i table = _mm_loadu_si128((const __m128i *)s->nib);
    __m128i bits = _mm_setr_epi8(
        1, 2, 4, 8, 16, 32, 64, (char)128, 1, 2, 4, 8, 16, 32, 64, (char)128
    );

    __m128i vec = _mm_loadu_si128((const __m128i *)buffer);
    __m128i lo = _mm_and_si128(vec, _mm_set1_epi8(0x0F));
    __m128i hi = _mm_and_si128(_mm_srli_epi16(vec, 4), _mm_set1_epi8(0x0F));

    __m128i rows = _mm_shuffle_epi8(table, lo);
    __m128i cols = _mm_shuffle_epi8(bits, hi);
    __m128i test = _mm_and_si128(rows, cols);

    /* a byte is a member when `rows & cols` keeps the selected bit */
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(test, cols));
}
#endif

static struct {
    size_t size; /* vector width in bytes, 0 when SIMD is unavailable */
    uint32_t (*match_set)(const char *buffer, const pstr_set_t *set);
    uint32_t (*match_chr)(const char *buffer, int ch);
    uint32_t (*compare)(const char *left, const char *right);
} g_impl = {
#if !defined(PSTRING_DETECT) && defined(PSTRING_AVX)
    .size = 32,
    .match_set = &pstr__match_set_avx,
    .match_chr = &pstr__match_chr_avx,
    .compare = &pstr__compare_avx,
#elif !defined(PSTRING_DETECT) && defined(PSTRING_SSE)
    .size = 16,
    #ifdef PSTRING_SSSE3
    .match_set = &pstr__match_set_sse,
    #else
    .match_set = NULL,
    #endif
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
    #ifdef PSTRING_SSE
    if (PF_HAS_SSE) {
        g_impl.size = 16;
        g_impl.match_chr = &pstr__match_chr_sse;
        g_impl.compare = &pstr__compare_sse;
        #ifdef PSTRING_SSSE3
        g_impl.match_set = &pstr__match_set_sse;
        #endif
    }
    #endif
    #ifdef PSTRING_AVX
    if (PF_HAS_AVX) {
        g_impl.size = 32;
        g_impl.match_set = &pstr__match_set_avx;
        g_impl.match_chr = &pstr__match_chr_avx;
        g_impl.compare = &pstr__compare_avx;
    }
    #endif
#endif
}

/** Mask covering the `size` low bits, avoiding the undefined `1 << 32`. **/
static inline uint32_t pstr__width_mask(size_t size) {
    return size >= 32 ? UINT32_MAX : (uint32_t)((1u << size) - 1);
}

/** Index of the lowest set bit. `x` must be non-zero. **/
static inline int pstr__first_bit(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(x);
#else
    int n = 0;
    while (!(x & 1u)) {
        x >>= 1;
        n++;
    }
    return n;
#endif
}

/** Index of the highest set bit, i.e. the last matching byte in a block.
    `x` must be non-zero. **/
static inline int pstr__last_bit(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return 31 - __builtin_clz(x);
#else
    int n = 0;
    while (x >>= 1)
        n++;
    return n;
#endif
}

int pstrequal(const pstring_t *left, const pstring_t *right) {
    if (left == right)
        return PSTRING_TRUE;

    size_t length = pstrlen(left);
    if (length != pstrlen(right))
        return PSTRING_FALSE;

    const char *leftBuf = pstrbuf(left);
    const char *rightBuf = pstrbuf(right);
    size_t i = 0;

    if (g_impl.size > 0) {
        uint32_t mask = pstr__width_mask(g_impl.size);

        for (; length - i >= g_impl.size; i += g_impl.size)
            if (mask != (g_impl.compare(&leftBuf[i], &rightBuf[i]) & mask))
                return PSTRING_FALSE;
    }

    for (; i < length; i++)
        if (leftBuf[i] != rightBuf[i])
            return PSTRING_FALSE;

    return PSTRING_TRUE;
}

int pstrcmp(const pstring_t *left, const pstring_t *right) {
    if (left == right)
        return 0;

    size_t llen = pstrlen(left);
    size_t rlen = pstrlen(right);
    size_t length = PF_MIN(llen, rlen);

    const unsigned char *leftBuf = (const unsigned char *)pstrbuf(left);
    const unsigned char *rightBuf = (const unsigned char *)pstrbuf(right);
    size_t i = 0;

    if (g_impl.size > 0) {
        uint32_t mask = pstr__width_mask(g_impl.size);

        for (; length - i >= g_impl.size; i += g_impl.size) {
            uint32_t equal = g_impl.compare(
                (const char *)&leftBuf[i], (const char *)&rightBuf[i]
            );
            uint32_t diff = ~equal & mask;

            if (diff) {
                /* movemask bit 0 is the first byte, so the earliest
                   difference is the lowest set bit, not the highest */
                size_t at = i + pf_ctz32(diff);
                return leftBuf[at] - rightBuf[at];
            }
        }
    }

    for (; i < length; i++)
        if (leftBuf[i] != rightBuf[i])
            return leftBuf[i] - rightBuf[i];

    /* a proper prefix sorts before the longer string */
    if (llen != rlen)
        return llen < rlen ? -1 : 1;

    return 0;
}

char *pstrchr(const pstring_t *str, int ch) {
    if (!str)
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    size_t length = pstrlen(str);
    char *buffer = pstrbuf(str);
    size_t i = 0;

    if (g_impl.size > 0) {
        for (; length - i >= g_impl.size; i += g_impl.size) {
            uint32_t result = g_impl.match_chr(&buffer[i], ch);
            if (result)
                return &buffer[i + pf_ctz32(result)];
        }
    }

    for (; i < length; i++)
        if (buffer[i] == (char)ch)
            return &buffer[i];

    return NULL;
}

char *pstrrchr(const pstring_t *str, int ch) {
    if (!str)
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    size_t length = pstrlen(str);
    char *buffer = pstrbuf(str);
    size_t left = length;

    if (g_impl.size > 0) {
        for (; left >= g_impl.size; left -= g_impl.size) {
            const char *slot = &buffer[left - g_impl.size];
            uint32_t result = g_impl.match_chr(slot, ch);
            if (result)
                return (char *)&slot[pstr__last_bit(result)];
        }
    }

    while (left-- > 0)
        if (buffer[left] == (char)ch)
            return &buffer[left];

    return NULL;
}

/** Scans forward for the first byte whose set membership equals `wanted`,
    returning the index of that byte or `length` when there is none. **/
static size_t pstr__scan(
    const char *buffer, size_t length, const pstr_set_t *set, int wanted
) {
    size_t i = 0;

    if (g_impl.size > 0 && g_impl.match_set) {
        uint32_t keep = pstr__width_mask(g_impl.size);

        for (; length - i >= g_impl.size; i += g_impl.size) {
            uint32_t hits = g_impl.match_set(&buffer[i], set) & keep;
            uint32_t result = wanted ? hits : (~hits & keep);

            if (result)
                return i + pf_ctz32(result);
        }
    }

    for (; i < length; i++)
        if (pstr__set_test(set, buffer[i]) == !!wanted)
            return i;

    return length;
}

/** Reverse counterpart of `pstr__scan`. Returns the index of the last byte
    whose membership equals `wanted`, or `length` when there is none. **/
static size_t pstr__rscan(
    const char *buffer, size_t length, const pstr_set_t *set, int wanted
) {
    size_t left = length;

    if (g_impl.size > 0 && g_impl.match_set) {
        uint32_t keep = pstr__width_mask(g_impl.size);

        for (; left >= g_impl.size; left -= g_impl.size) {
            const char *slot = &buffer[left - g_impl.size];
            uint32_t hits = g_impl.match_set(slot, set) & keep;
            uint32_t result = wanted ? hits : (~hits & keep);

            if (result)
                return (left - g_impl.size) + pstr__last_bit(result);
        }
    }

    while (left-- > 0)
        if (pstr__set_test(set, buffer[left]) == !!wanted)
            return left;

    return length;
}

/** Compiles `set` and reports whether the string is usable. **/
static int pstr__set_from(pstr_set_t *out, const char *set) {
    if (!set)
        return 0;
    pstr__set_build(out, set, pstr__nlen(set, PSTRING_MAX_SET));
    return 1;
}

size_t pstrspn(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return 0;

    /* number of leading bytes that are members: index of the first non-member */
    return pstr__scan(pstrbuf(str), pstrlen(str), &compiled, 0);
}

size_t pstrcspn(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return 0;

    return pstr__scan(pstrbuf(str), pstrlen(str), &compiled, 1);
}

size_t pstrrspn(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return 0;

    size_t length = pstrlen(str);
    size_t at = pstr__rscan(pstrbuf(str), length, &compiled, 0);

    /* distance from the end back to the last non-member */
    return at == length ? length : length - at - 1;
}

size_t pstrrcspn(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return 0;

    size_t length = pstrlen(str);
    size_t at = pstr__rscan(pstrbuf(str), length, &compiled, 1);

    return at == length ? length : length - at - 1;
}

char *pstrpbrk(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    size_t length = pstrlen(str);
    size_t at = pstr__scan(pstrbuf(str), length, &compiled, 1);
    return at == length ? NULL : &pstrbuf(str)[at];
}

char *pstrcpbrk(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    size_t length = pstrlen(str);
    size_t at = pstr__scan(pstrbuf(str), length, &compiled, 0);
    return at == length ? NULL : &pstrbuf(str)[at];
}

char *pstrrpbrk(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    size_t length = pstrlen(str);
    size_t at = pstr__rscan(pstrbuf(str), length, &compiled, 1);
    return at == length ? NULL : &pstrbuf(str)[at];
}

char *pstrrcpbrk(const pstring_t *str, const char *set) {
    pstr_set_t compiled;
    if (!str || !pstr__set_from(&compiled, set))
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    size_t length = pstrlen(str);
    size_t at = pstr__rscan(pstrbuf(str), length, &compiled, 0);
    return at == length ? NULL : &pstrbuf(str)[at];
}