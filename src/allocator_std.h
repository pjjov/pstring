/** `allocator_t` - Interface for custom allocators in C.

    This file provides a wrapper for the standard library
    allocation functions using the `allocator_t` interface.

    Aligned allocations are implemented with C11's `aligned_alloc`
    or `posix_memalign`, depending on their availability.

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
**/

#ifndef ALLOCATOR_STD
#define ALLOCATOR_STD

#ifndef ALLOCATOR_T
#include "allocator.h"
#endif

#include <stdlib.h>
#include <string.h>

static void *standard_allocator_fn(allocator_t *self, void *ptr, size_t old, size_t size, size_t zalign) {
    if (self == ptr)
        return NULL;
    void *out = NULL;

    if (size > 0) {
        if (ptr) {
            if (zalign > 1) {
                if (old == 0)
                    return NULL;

            #if __STDC_VERSION__ >= 201112L
                out = aligned_alloc(zalign & ~1, size);
            #elif _POSIX_C_SOURCE >= 200112L
                if (posix_memalign(&out, zalign & ~1, size))
                    return NULL;
            #endif

                if (out) {
                    memcpy(out, ptr, old < size ? old : size);
                    free(ptr);
                }
                return out;
            } else {
                out = realloc(ptr, size);
            }

            if (out && zalign & 1 && old < size && old > 0) {
                memset(
                    &((unsigned char *)out)[old],
                    0, size - old
                );
            }
            return out;
        }

        if(zalign > 1) {

        #if __STDC_VERSION__ >= 201112L
            out = aligned_alloc(zalign & ~1, size);
        #elif _POSIX_C_SOURCE >= 200112L
            if (posix_memalign(&out, zalign & ~1, size))
                return NULL;
        #endif

            if (out && zalign & 1)
                memset(out, 0, size);
            return out;
        }

        return zalign & 1 ? calloc(1, size) : malloc(size);
    }

    if (ptr)
        free(ptr);
    return out;
}

static allocator_t standard_allocator = { &standard_allocator_fn };

#endif
