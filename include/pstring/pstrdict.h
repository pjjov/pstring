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

#ifndef PSTRING_DICT_H
#define PSTRING_DICT_H

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#include <stddef.h>

typedef struct allocator_t allocator_t;
typedef struct pstring_t pstring_t;

/** `pstrdict_t` is a hashdict that stores key-value pairs, where the key is a
    `pstring` and the value is a `void` pointer. Since both keys and values are
    stored as pointers, no memory management is done for them.
**/
typedef struct pstrdict_t pstrdict_t;
typedef size_t(pstrhash_fn)(const pstring_t *str);

/** Allocates a new `pstrdict` using the `allocator` that will produce key hashes
    using the provided `hash` function. For each of the parameters that are
    `NULL`, the default ones will be used.
**/
PSTR_API pstrdict_t *pstrdict_new(pstrhash_fn *hash, allocator_t *allocator);

/** Reserves space to fit at least `count` more key-value pairs inside `dict`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdict_reserve(pstrdict_t *dict, size_t count);

/** Frees all memory resources used by `dict`. **/
PSTR_API void pstrdict_free(pstrdict_t *dict);

/** Returns the number of slots reserved in `dict` **/
PSTR_API size_t pstrdict_capacity(const pstrdict_t *dict);

/** Returns the number of key-value pairs in `dict` **/
PSTR_API size_t pstrdict_count(const pstrdict_t *dict);

/** Returns the allocator used by `dict` **/
PSTR_API allocator_t *pstrdict_allocator(const pstrdict_t *dict);

/** Removes all key-value pairs from `dict` **/
PSTR_API void pstrdict_clear(pstrdict_t *dict);

/** Retrieves the value associated with `key` or `NULL` if not found. **/
PSTR_API void *pstrdict_get(pstrdict_t *dict, const pstring_t *key);

/** Sets the value associated with `key` to `value`, inserting them if `key` is
    not already present in `dict`. Both pointers, the initial key and current
    value, must remain valid until they are removed or `dict` is destroyed.

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdict_set(
    pstrdict_t *dict, const pstring_t *key, const void *value
);

/** Inserts the key-value pair if it's not already present in `dict`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EEXIST.
**/
PSTR_API int pstrdict_insert(
    pstrdict_t *dict, const pstring_t *key, const void *value
);

/** Removes the key-value pair from `dict`, if it's found.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_ENOENT.
**/
PSTR_API int pstrdict_remove(pstrdict_t *dict, const pstring_t *key);

/** Forcefully inserts the key-value pair, without checking for it's presence.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrdict_finsert(
    pstrdict_t *dict, const pstring_t *key, const void *value
);

/** Callback that traverses the key-value pairs of a dictionary. **/
typedef int (pstrdict_fn)(void *user, pstring_t *key, void *value);

/** Calls `fn` for each key-value pair inside `dict`.
    If a non-zero value is returned by `fn`, the iteration is interrupted.

    Possible error codes: PSTRING_EINVAL, PSTRING_EINTR.
**/
PSTR_API int pstrdict_each(pstrdict_t *dict, pstrdict_fn *fn, void *user);

/** Calls `fn` for each key-value pair inside `dict`,
    removing them if `PSTRING_FALSE` is returned.

    Possible error codes: PSTRING_EINVAL, PSTRING_EINTR.
**/
PSTR_API int pstrdict_filter(pstrdict_t *dict, pstrdict_fn *fn, void *user);

#endif
