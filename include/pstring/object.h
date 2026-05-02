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

#ifndef PSTRING_OBJECT_H
#define PSTRING_OBJECT_H

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

/** ## NAME

    **pstring-object** - dynamic object handling and serialization.

    ## DESCRIPTION

    [TOC]

    ## REFERENCE
**/

#include <stddef.h>
typedef struct allocator_t allocator_t;
typedef struct pstring_t pstring_t;
typedef struct pstream_t pstream_t;
typedef struct pstrobj_t pstrobj_t;

enum pstrobj_type {
    PSTROBJ_NULL,
    PSTROBJ_BOOL,
    PSTROBJ_LONG,
    PSTROBJ_DOUBLE,
    PSTROBJ_STRING,
    PSTROBJ_ARRAY,
    PSTROBJ_DICT,
};

enum pstrobj_flag {
    PSTROBJ_FLAG_ROOT = 1,
    PSTROBJ_FLAG_ARENA = 2,
    PSTROBJ_FLAG_WRAP = 3,
};

struct pstrobj_t {
    pstrobj_t *next;
    pstrobj_t *prev;
    pstrobj_t *child;
    allocator_t *allocator;

    int type;
    int flags;

    union {
        char bool_;
        long long_;
        double double_;
        pstring_t *string;
    } as;
};

/** Allocates a new object with NULL type. **/
PSTR_API pstrobj_t *pstrobj_new(allocator_t *allocator);

/** Loads an object from `source` that is in specified `format`. **/
PSTR_API pstrobj_t *pstrobj_from_buffer(
    const char *format, pstring_t *source, allocator_t *allocator
);

/** Loads an object in `format` by reading from `stream`. **/
PSTR_API pstrobj_t *pstrobj_from_stream(
    const char *format, pstream_t *stream, allocator_t *allocator
);

/** Frees object and it's children if it's detached. **/
PSTR_API void pstrobj_free(pstrobj_t *obj);

/** Following functions set the object's value and type.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_set_null(pstrobj_t *obj);
PSTR_API int pstrobj_set_bool(pstrobj_t *obj, char value);
PSTR_API int pstrobj_set_int(pstrobj_t *obj, int value);
PSTR_API int pstrobj_set_long(pstrobj_t *obj, long value);
PSTR_API int pstrobj_set_float(pstrobj_t *obj, float value);
PSTR_API int pstrobj_set_double(pstrobj_t *obj, double value);

/** Copies the contents of `str` as the value of `obj`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_copy_string(pstrobj_t *obj, const char *str, size_t len);
PSTR_API int pstrobj_copy_pstring(pstrobj_t *obj, const pstring_t *str);

/** Wraps the contents of `str` as the value of `obj`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_wrap_string(pstrobj_t *obj, const char *str, size_t len);
PSTR_API int pstrobj_wrap_pstring(pstrobj_t *obj, const pstring_t *str);

#endif
