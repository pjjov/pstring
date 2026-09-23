/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_IO_H
#define PSTRING_IO_H

#ifndef PSTR_INLINE
    #define PSTR_INLINE static inline
#endif

#ifndef PSTR_API
    #define PSTR_API
#endif

#include <stddef.h>
#include <stdio.h>

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

/* Forward declarations */
typedef struct allocator_t allocator_t;
typedef struct pstring_t pstring_t;
typedef struct pf_stream_t pf_stream_t;

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

/** Concatenates the contents of the file onto the end of `out`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
PSTR_API int pstrreadall(pstring_t *out, const char *path);

/** Writes the entire string `out` to the file at `path`.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
PSTR_API int pstrwriteall(const pstring_t *str, const char *path);

/** Concatenates string formated by standard library functions to `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
PSTR_API int pstrio_printf(pstring_t *dst, const char *fmt, ...);

/** Concatenates string formated by standard library functions to `dst`.
    Arguments are passed as a variable arguments list from `<stdarg.h>`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
PSTR_API int pstrio_vprintf(pstring_t *dst, const char *fmt, va_list args);

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
