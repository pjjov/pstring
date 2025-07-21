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

#include "allocator_std.h"

#define PSTRDICT_BUCKET_SIZE 16
#define PSTRDICT_THRESHOLD 0.7

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
    pstrhash_fn *hash;
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

/* graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2 */
static inline size_t round_pow2(size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#if SIZE_MAX > UINT32_MAX
    v |= v >> 32;
#endif
    v++;

    return v;
}

static size_t default_hash(const pstring_t *str) { return 0; }

pstrdict_t *pstrdict_new(pstrhash_fn *hash, allocator_t *allocator) {
    if (!allocator)
        allocator = &standard_allocator;
    if (!hash)
        hash = &default_hash;

    pstrdict_t *out = allocate(allocator, sizeof(pstrdict_t));

    if (out) {
        out->buckets = NULL;
        out->count = 0;
        out->capacity = 0;
        out->hash = hash;
        out->allocator = allocator;
    }

    return out;
}

void pstrdict_clear(pstrdict_t *dict) {
    if (!dict)
        return;

    dict->count = 0;
    for (size_t b = 0; b < dict->capacity / PSTRDICT_BUCKET_SIZE; b++) {
        union metadata *meta = &dict->buckets[b].meta;
        memset(meta, 0, sizeof(union metadata));
    }
}

static int grow_empty(pstrdict_t *dict, size_t capacity) {
    struct bucket *buckets = reallocate(
        dict->allocator,
        dict->buckets,
        dict->capacity / PSTRDICT_BUCKET_SIZE * sizeof(struct bucket),
        capacity / PSTRDICT_BUCKET_SIZE * sizeof(struct bucket)
    );

    if (!buckets)
        return PSTRING_ENOMEM;

    dict->buckets = buckets;
    dict->capacity = capacity;
    pstrdict_clear(dict);
    return PSTRING_OK;
}

int pstrdict_reserve(pstrdict_t *dict, size_t count) {
    if (!dict)
        return PSTRING_EINVAL;

    if (dict->count + count <= dict->capacity * PSTRDICT_THRESHOLD)
        return PSTRING_OK;

    size_t capacity = round_pow2(dict->capacity) * 2;
    if (capacity < PSTRDICT_BUCKET_SIZE)
        capacity = PSTRDICT_BUCKET_SIZE;

    if (dict->count == 0)
        return grow_empty(dict, capacity);
    return PSTRING_ENOMEM;
}

void pstrdict_free(pstrdict_t *dict) {
    if (dict) {
        deallocate(
            dict->allocator,
            dict->buckets,
            dict->capacity / PSTRDICT_BUCKET_SIZE * sizeof(struct bucket)
        );

        deallocate(dict->allocator, dict, sizeof(pstrdict_t));
    }
}

size_t pstrdict_capacity(const pstrdict_t *dict) {
    return dict ? dict->capacity : 0;
}

size_t pstrdict_count(const pstrdict_t *dict) { return dict ? dict->count : 0; }

allocator_t *pstrdict_allocator(const pstrdict_t *dict) {
    return dict ? dict->allocator : 0;
}
