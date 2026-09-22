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

#ifndef PSTRING_IO_H
#define PSTRING_IO_H

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** ## NAME

    **pstring-io** - streaming and serialization functions for **pstrings**.

    ## DESCRIPTION

    The `pf_stream_t` object can be used as a generic interface for interacting
    with byte streams. Unlike standard library `FILE`, `pf_stream_t` objects
    can use user-defined stream implementations such as network sockets,
    in-memory buffers, encoders and serializers.

    This header also provides powerful serialization and deserialization
    mechanisms through the `pstrmodel` and associated structures.

    [TOC]

    ## REFERENCE
**/

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
typedef struct allocator_t allocator_t;
typedef struct pstring_t pstring_t;
typedef struct pf_stream_t pf_stream_t;

#define PSTREAM_STATE_SIZE 24

enum pstream_origin {
    PSTR_SEEK_SET,
    PSTR_SEEK_CUR,
    PSTR_SEEK_END,
};

enum pstring_typeid {
    PSTRING_TYPE = 3 | ('P' << 8),
    PSTRING_PTR_TYPE,
    PSTRMODEL_TYPE,
    PSTRMODEL_ARRAY,
    PSTRMODEL_LLIST,

    PSTRMODEL__KEY,
    PSTRMODEL__BEGIN,
    PSTRMODEL__END,
};

/** Concatenates string formated by standard library functions to `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
PSTR_API int pstrio_printf(pstring_t *dst, const char *fmt, ...);

/** Concatenates string formated by standard library functions to `dst`.
    Arguments are passed as a variable arguments list from `<stdarg.h>`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
PSTR_API int pstrio_vprintf(pstring_t *dst, const char *fmt, va_list args);

PSTR_API int pstrfprintf(pf_stream_t *stream, const char *fmt, ...);
PSTR_API int pstrvfprintf(pf_stream_t *stream, const char *fmt, va_list args);

struct pstrmodel_array {
    size_t stride;
    size_t count;
    struct pstrmodel *submodel;
};

struct pstrmodel_llist {
    size_t linkOffset;
    allocator_t *allocator;
    struct pstrmodel *submodel;
};

struct pstrmodel_member {
    const char *name;
    int type;
    size_t offset;
    void *model;
};

struct pstrmodel {
    const char *name;
    struct pstrmodel_member *members;
};

typedef int(pstream_save_fn)(
    pf_stream_t *stream, const void *obj, const struct pstrmodel *model
);

typedef int(pstream_load_fn)(
    pf_stream_t *stream, void *obj, const struct pstrmodel *model
);

/** Initializes a stream that will read and write to the buffer
    of `str`, expanding it if needed. `str` will NOT be freed by
    the stream when closing.

    Stream cursor will be at the end of the string.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
PSTR_API int pf_stream_pstring(pf_stream_t *out, pstring_t *str);

/** Writes a `pstring_t` to `stream`.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
PSTR_API int pf_stream_putp(pf_stream_t *stream, const pstring_t *str);

PSTR_API int pstream_save(
    const char *format,
    pf_stream_t *stream,
    const void *obj,
    const struct pstrmodel *model
);

PSTR_API int pstream_load(
    const char *format,
    pf_stream_t *stream,
    void *obj,
    const struct pstrmodel *model
);

PSTR_API int pstream_save_json(
    pf_stream_t *stream, const void *obj, const struct pstrmodel *model
);

PSTR_API int pstream_load_json(
    pf_stream_t *stream, void *obj, const struct pstrmodel *model
);

#ifdef __cplusplus
}
#endif

#endif
