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

#ifndef PSTRING_IO_H
#define PSTRING_IO_H

#include <pf_typeid.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
typedef struct pstring_t pstring_t;
typedef struct pstream_t pstream_t;

#define PSTREAM_STATE_SIZE 24

enum pstream_origin {
    PSTRING_SEEK_SET,
    PSTRING_SEEK_CUR,
    PSTRING_SEEK_END,
};

enum pstring_typeid {
    PSTRTYPE__NAMESPACE = 'P' << 8,
    PSTRTYPE_ARRAY,
    PSTRTYPE_MAP,
    PSTRTYPE__MAX,
};

/** Concatenates string formated by standard library functions to `dst`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
int pstrio_printf(pstring_t *dst, const char *fmt, ...);

/** Concatenates string formated by standard library functions to `dst`.
    Arguments are passed as a variable arguments list from `<stdarg.h>`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM, PSTRING_EIO.
**/
int pstrio_vprintf(pstring_t *dst, const char *fmt, va_list args);

struct pstream_vt {
    size_t (*read)(pstream_t *stream, void *buffer, size_t size);
    size_t (*write)(pstream_t *stream, void *buffer, size_t size);
    size_t (*tell)(pstream_t *stream);
    int (*seek)(pstream_t *stream, long offset, int origin);
    void (*flush)(pstream_t *stream);
    void (*close)(pstream_t *stream);
    int (*serialize)(pstream_t *stream, int type, const void *item);
    int (*deserialize)(pstream_t *stream, int type, void *item);
};

struct pstream_t {
    const struct pstream_vt *vtable;
    union {
        char _size[PSTREAM_STATE_SIZE];
        void *_align;
        void *ptr[PSTREAM_STATE_SIZE / sizeof(void *)];
    } state;
};

/** Opens the file located at `path` as a stream.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
int pstream_open(pstream_t *out, const char *path, const char *mode);

/** Wraps provided `file` handle into a stream.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
int pstream_file(pstream_t *out, FILE *file);

/** Initializes a stream that will read and write to the buffer
    of `str`, expanding it if needed. `str` will NOT be freed by
    the stream when closing.

    Stream cursor will be at the end of the string.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
int pstream_string(pstream_t *out, pstring_t *str);

/** Initializes `out` as a custom stream.
    `vtable` and it's members cannot be `NULL`.

    Possible error codes: PSTRING_EINVAL.
**/
int pstream_init(pstream_t *out, const struct pstream_vt *vtable);

/*
    Since `pstream_init` does `NULL` checking for vtable members before
    any stream is used, checking for `NULL` again in each of the helper
    functions is unnecessary and a real performance drop.

    As such, all helper functions checks are done through PSTREAM_ASSERT
    which can be removed by defining PSTREAM_NDEBUG.
*/
#ifndef PSTREAM_NDEBUG
    #define PSTREAM_ASSERT(m_fn, m_ret)                          \
        if (!(stream && stream->vtable && stream->vtable->m_fn)) \
            return (m_ret);
#else
    #define PSTREAM_ASSERT(m_fn, m_ret)
#endif

/** Sets the position of the stream to an offset of the specified origin.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
static inline int pstream_seek(pstream_t *stream, long offset, int origin) {
    PSTREAM_ASSERT(seek, 0);
    return stream->vtable->seek(stream, offset, origin);
}

/** Returns the current position of the stream. **/
static inline size_t pstream_tell(pstream_t *stream) {
    PSTREAM_ASSERT(tell, 0);
    return stream->vtable->tell(stream);
}

/** Reads up to `size` bytes from `stream` and returns number of bytes read.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
static inline size_t
pstream_read(pstream_t *stream, void *buffer, size_t size) {
    PSTREAM_ASSERT(tell, 0);
    return stream->vtable->read(stream, buffer, size);
}

/** Attempts to write `size` bytes from `buffer` to `stream`.
    The number of bytes actually written is returned.
    Possible error codes: PSTRING_EINVAL, PSTRING_EIO.
**/
static inline size_t
pstream_write(pstream_t *stream, void *buffer, size_t size) {
    PSTREAM_ASSERT(tell, 0);
    return stream->vtable->tell(stream);
}

/** Flushes internal buffers of a stream. **/
static inline void pstream_flush(pstream_t *stream) {
    PSTREAM_ASSERT(flush, (void)0);
    stream->vtable->flush(stream);
}

/** Closes the stream and frees it's resources. **/
static inline void pstream_close(pstream_t *stream) {
    PSTREAM_ASSERT(close, (void)0);
    return stream->vtable->close(stream);
}

static inline int
pstream_serialize(pstream_t *stream, int type, const void *item) {
    PSTREAM_ASSERT(serialize, -22);
    return stream->vtable->serialize(stream, type, item);
}

static inline int pstream_deserialize(pstream_t *stream, int type, void *item) {
    PSTREAM_ASSERT(deserialize, -22);
    return stream->vtable->deserialize(stream, type, item);
}

#endif
