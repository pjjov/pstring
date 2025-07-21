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
#include <pstring/pstrdict.h>

#include <stdint.h>

#define PSTRDICT_BUCKET_SIZE 16

#if !defined(PSTRING_NO_SIMD) && defined(__SSE2__) && PSTRDICT_BUCKET_SIZE >= 16
    #define PSTRDICT_SSE2
    #include <emmintrin.h>
#endif

struct pair {
    const pstring_t *key;
    const void *value;
};

union metadata {
    uint8_t hashes[PSTRDICT_BUCKET_SIZE];
#ifdef PSTRDICT_SSE2
    __m128i sse[PSTRDICT_BUCKET_SIZE / 16];
#endif
};

struct bucket {
    union metadata meta;
    struct pair pairs[PSTRDICT_BUCKET_SIZE];
};

struct pstrdict_t {
    struct bucket *buckets;
    size_t count;
    size_t capacity;
    allocator_t *allocator;
};

static inline uint64_t bucket_match(union metadata *meta, uint8_t part) {
    uint64_t out = 0;
#ifdef PSTRDICT_SSE2
    __m128i tmp = _mm_set1_epi8(part);
    for (int i = 0; i < PSTRDICT_BUCKET_SIZE / 16; i++) {
        int result = _mm_movemask_epi8(_mm_cmpeq_epi8(meta->sse[i], tmp));
        out |= result << (i * 16);
    }
#else
    for (int i = 0; i < PSTRDICT_BUCKET_SIZE; i++) {
        if (meta->parts[i] == part)
            out |= 1 << i;
    }
#endif
    return out;
}
