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

#include <stddef.h>

typedef struct allocator_t allocator_t;
typedef struct pstring_t pstring_t;

/** `pstrdict_t` is a hashdict that stores key-value pairs, where the key is a
    `pstring` and the value is a `void` pointer. Since both keys and values are
    stored as pointers, no memory management is done for them.
**/
typedef struct pstrdict_t pstrdict_t;
typedef size_t(pstrhash_fn)(const pstring_t *str);

#endif
