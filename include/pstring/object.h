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

    `pstrobj_t` represents a dynamically typed object which can be saved to and
    loaded from various formats. Unlike `struct pstrmodel` which maps objects
    directly to C structures, `pstrobj` maintains the whole object in memory.

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

typedef pstrobj_t *(pstrobj_load_fn)(pstream_t * stream, allocator_t *alloc);
typedef int(pstrobj_save_fn)(pstrobj_t *obj, pstream_t *stream);

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
PSTR_API pstrobj_t *pstrobj_load_json(
    pstream_t *stream, allocator_t *allocator
);

/** Loads an object in `format` by reading from the file at `path`. **/
PSTR_API pstrobj_t *pstrobj_from_path(
    const char *format, const char *path, allocator_t *allocator
);

/** Saves an object to `source` in specified `format`. **/
PSTR_API int pstrobj_to_buffer(
    pstrobj_t *obj, const char *format, pstring_t *source
);

/** Saves an object in `format` by writing to `stream`. **/
PSTR_API int pstrobj_to_stream(
    pstrobj_t *obj, const char *format, pstream_t *stream
);
PSTR_API int pstrobj_save_json(pstrobj_t *obj, pstream_t *stream);

/** Frees object and it's children if it's detached. **/
PSTR_API void pstrobj_free(pstrobj_t *obj);

/** Finds an object using the JSON Pointer format. **/
PSTR_API pstrobj_t *pstrobj_query(pstrobj_t *obj, const char *query);
PSTR_API void *pstrobj_query_value(pstrobj_t *obj, const char *query);

#define PSTROBJ_FOREACH(OBJ, CHILD)                                    \
    for (CHILD = ((OBJ) != NULL ? (OBJ)->child : NULL); CHILD != NULL; \
         CHILD = (CHILD)->next)

/** nanodoc.inline-decl on **/

/** ### pstrobj_set_*

    Following functions set the object's value and type.
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

/** ### pstrobj_expect_*

    Following functions return the object's value if `obj` has a matching type.
    Otherwise a default value is returned and `status` is set to an error code.
**/
PSTR_API void pstrobj_expect_null(pstrobj_t *obj, int *status);
PSTR_API int pstrobj_expect_bool(pstrobj_t *obj, int *status);
PSTR_API int pstrobj_expect_int(pstrobj_t *obj, int *status);
PSTR_API long pstrobj_expect_long(pstrobj_t *obj, int *status);
PSTR_API float pstrobj_expect_float(pstrobj_t *obj, int *status);
PSTR_API double pstrobj_expect_double(pstrobj_t *obj, int *status);
PSTR_API const char *pstrobj_expect_string(pstrobj_t *obj, int *status);
PSTR_API pstring_t *pstrobj_expect_pstring(pstrobj_t *obj, int *status);
PSTR_API pstrobj_t *pstrobj_expect_list(pstrobj_t *obj, int *status);
PSTR_API pstrobj_t *pstrobj_expect_dict(pstrobj_t *obj, int *status);

/** ### pstrobj_query_*

    Following functions query `obj` for a child and return the child's value
    if `obj` has a matching type. Otherwise a default value is returned and
    `status` is set to an error code.
**/
#define PSTROBJ__IMPL_QUERY(NAME, TYPE)                                  \
    PSTR_INLINE TYPE pstrobj_query_##NAME(                               \
        pstrobj_t *obj, const char *query, int *status                   \
    ) {                                                                  \
        return pstrobj_expect_##NAME(pstrobj_query(obj, query), status); \
    }
PSTROBJ__IMPL_QUERY(bool, int);
PSTROBJ__IMPL_QUERY(int, int);
PSTROBJ__IMPL_QUERY(long, long);
PSTROBJ__IMPL_QUERY(float, float);
PSTROBJ__IMPL_QUERY(double, double);
PSTROBJ__IMPL_QUERY(string, const char *);
PSTROBJ__IMPL_QUERY(pstring, pstring_t *);
PSTROBJ__IMPL_QUERY(list, pstrobj_t *);
PSTROBJ__IMPL_QUERY(dict, pstrobj_t *);
#undef PSTROBJ__IMPL_QUERY

/** ### pstrobj_set_*

    Following functions return the object's value or the default value `def`,
    depending on the type of `obj`, converting between types if necessary.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
**/
PSTR_API int pstrobj_get_int(pstrobj_t *obj, int def);
PSTR_API long pstrobj_get_long(pstrobj_t *obj, long def);
PSTR_API float pstrobj_get_float(pstrobj_t *obj, float def);
PSTR_API double pstrobj_get_double(pstrobj_t *obj, double def);

/** nanodoc.inline-decl off **/

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

/** Finds an item in `dict` with specified `key`. **/
PSTR_API pstrobj_t *pstrobj_dict_get(pstrobj_t *dict, const pstring_t *key);
PSTR_API pstrobj_t *pstrobj_dict_gets(
    pstrobj_t *dict, const char *key, size_t length
);

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

/* Forcefully sets the string or key by dereferencing `str`. */
void pstrobj__set_string(pstrobj_t *obj, pstring_t *str);
void pstrobj__set_key(pstrobj_t *obj, pstring_t *key);

#endif
