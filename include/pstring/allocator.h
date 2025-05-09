/** `allocator_t` - Interface for custom allocators in C.

    `allocator_t` is an interface for using and creating custom allocators
    that enables you to swap in arenas, pools and debug allocators into
    existing C libraries, thus helping you write safer and faster code.

    With `allocator_t` you can break the global allocator paradigm
    present in C and use the right tool for each situation.

    You can use `allocator_t` with existing C libraries through
    wrapper headers that are provided alongside this one.

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

#ifndef ALLOCATOR_T
#define ALLOCATOR_T
#include <stddef.h>

typedef struct allocator_t allocator_t;

typedef void *(allocator_fn)(
    allocator_t *self,
    void *ptr,
    size_t old,
    size_t size,
    size_t zalign
);

struct allocator_t {
    allocator_fn *const interface;
};

#ifndef allocator_failure
    #include <stdlib.h>
    #define allocator_failure(m_alloc, m_ptr, m_size, m_old, m_zalign) abort()
#endif
#ifndef allocator_default
    #define allocator_default(m_ptr, m_size, m_old, m_zalign) NULL
#endif

static void *allocator_call(allocator_t *alloc, void *ptr, size_t old, size_t size, size_t zalign) {
    if (!alloc || !alloc->interface) {
        return allocator_default(ptr, old, size, zalign);
    }
    return alloc->interface(alloc, ptr, old, size, zalign);
}

static void *allocator_callx(allocator_t *alloc, void *ptr, size_t old, size_t size, size_t zalign) {
    void *res = allocator_call(alloc, ptr, old, size, zalign);
    if (res == NULL) {
        allocator_failure(alloc, ptr, old, size, zalign);
    }
    return res;
}

static inline void *allocate(allocator_t *alloc, size_t size) {
    return allocator_call(alloc, NULL, 0, size, 0);
}

static inline void *allocate_aligned(allocator_t *alloc, size_t size, size_t align) {
    return allocator_call(alloc, NULL, 0, size, align & ~1);
}

static inline void *reallocate(allocator_t *alloc, void *ptr, size_t old, size_t size) {
    return allocator_call(alloc, ptr, old, size, 0);
}

static inline void *reallocate_aligned(allocator_t *alloc, void *ptr, size_t old, size_t size, size_t align) {
    return allocator_call(alloc, ptr, old, size, align & ~1);
}

static inline void *zallocate(allocator_t *alloc, size_t size) {
    return allocator_call(alloc, NULL, 0, size, 1);
}

static inline void *zallocate_aligned(allocator_t *alloc, size_t size, size_t align) {
    return allocator_call(alloc, NULL, 0, size, align | 1);
}

static inline void *zreallocate(allocator_t *alloc, void *ptr, size_t old, size_t size) {
    return allocator_call(alloc, ptr, old, size, 1);
}

static inline void *zreallocate_aligned(allocator_t *alloc, void *ptr, size_t old, size_t size, size_t align) {
    return allocator_call(alloc, ptr, old, size, align | 1);
}

static inline void deallocate(allocator_t *alloc, void *ptr, size_t old) {
    allocator_call(alloc, ptr, old, 0, 0);
}

static inline void *xallocate(allocator_t *alloc, size_t size) {
    return allocator_callx(alloc, NULL, 0, size, 0);
}

static inline void *xallocate_aligned(allocator_t *alloc, size_t size, size_t align) {
    return allocator_callx(alloc, NULL, 0, size, align);
}

static inline void *xreallocate(allocator_t *alloc, void *ptr, size_t old, size_t size) {
    return allocator_callx(alloc, ptr, old, size, 0);
}

static inline void *xreallocate_aligned(allocator_t *alloc, void *ptr, size_t old, size_t size, size_t align) {
    return allocator_callx(alloc, ptr, old, size, align);
}

static inline void *xzallocate(allocator_t *alloc, size_t size) {
    return allocator_callx(alloc, NULL, 0, size, 1);
}

static inline void *xzallocate_aligned(allocator_t *alloc, size_t size, size_t align) {
    return allocator_callx(alloc, NULL, 0, size, align | 1);
}

static inline void *xzreallocate(allocator_t *alloc, void *ptr, size_t old, size_t size) {
    return allocator_callx(alloc, ptr, old, size, 1);
}

static inline void *xzreallocate_aligned(allocator_t *alloc, void *ptr, size_t old, size_t size, size_t align) {
    return allocator_callx(alloc, ptr, old, size, align | 1);
}

static inline void deallocate_all(allocator_t *alloc) {
    allocator_call(alloc, (void *)alloc, 0, 0, 0);
}

#endif
