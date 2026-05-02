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

/* Copied to avoid header inclusion for inline functions */
enum pstrobj_error {
    PSTROBJ_OK = 0,
    PSTROBJ_ENOENT = -2,
    PSTROBJ_EINTR = -4,
    PSTROBJ_ENOMEM = -12,
    PSTROBJ_EEXIST = -17,
    PSTROBJ_EINVAL = -22,
};

enum pstrobj_type {
    PSTROBJ_NULL,
    PSTROBJ_BOOL,
    PSTROBJ_LONG,
    PSTROBJ_DOUBLE,
    PSTROBJ_STRING,
    PSTROBJ_LIST,
    PSTROBJ_DICT,
};

enum pstrobj_flag {
    PSTROBJ_FLAG_ROOT = 1,
    PSTROBJ_FLAG_ARENA = 2,
    PSTROBJ_FLAG_WRAP = 3,
    PSTROBJ_FLAG_WRAP_KEY = 4,
};

struct pstrobj_t {
    pstrobj_t *next;
    pstrobj_t *prev;
    pstrobj_t *child;
    allocator_t *allocator;

    pstring_t *key;
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
PSTR_API int pstrobj_set_list(pstrobj_t *obj);
PSTR_API int pstrobj_set_dict(pstrobj_t *obj);

/** Copies the contents of `str` as the value of `obj`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_copy_string(pstrobj_t *obj, const char *str, size_t len);
PSTR_API int pstrobj_copy_pstring(pstrobj_t *obj, const pstring_t *str);
PSTR_API int pstrobj_copy_key(pstrobj_t *obj, const pstring_t *str);
PSTR_API int pstrobj_copy_keys(pstrobj_t *obj, const char *str, size_t len);

/** Wraps the contents of `str` as the value of `obj`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_wrap_string(pstrobj_t *obj, const char *str, size_t len);
PSTR_API int pstrobj_wrap_pstring(pstrobj_t *obj, const pstring_t *str);
PSTR_API int pstrobj_wrap_key(pstrobj_t *obj, const pstring_t *str);
PSTR_API int pstrobj_wrap_keys(pstrobj_t *obj, const char *str, size_t len);

/** Inserts `item` at index `i` inside `list`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_list_insert(pstrobj_t *list, pstrobj_t *item, size_t i);

/** Removes item at index `i` in `list` and returns it. **/
PSTR_API pstrobj_t *pstrobj_list_remove(pstrobj_t *list, size_t i);

/** Removes item at index `i` in `list` and frees it.
    Possible error codes: PSTRING_EINVAL.
**/
PSTR_INLINE int pstrobj_list_free(pstrobj_t *list, size_t i) {
    pstrobj_t *obj = pstrobj_list_remove(list, i);
    pstrobj_free(obj);
    return obj ? PSTROBJ_OK : PSTROBJ_EINVAL;
}

/** Inserts `item` into `dict` with a previously set key.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_dict_insert(pstrobj_t *dict, pstrobj_t *item);

/** Removes item with specified key in `dict` and returns it. **/
PSTR_API pstrobj_t *pstrobj_dict_remove(
    pstrobj_t *dict, const char *key, size_t length
);

/** Removes item with specified key in `dict` and frees it.
    Possible error codes: PSTRING_EINVAL.
**/
PSTR_INLINE int pstrobj_dict_free(
    pstrobj_t *dict, const char *key, size_t length
) {
    pstrobj_t *obj = pstrobj_dict_remove(dict, key, length);
    pstrobj_free(obj);
    return obj ? PSTROBJ_OK : PSTROBJ_EINVAL;
}

#endif
