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

#include <pstring/core.h>
#include <pstring/encoding.h>
#include <pstring/io.h>

#define PF_TYPE_HELPERS
#include <pf_io.h>
#include <pf_macro.h>
#include <pf_typeid.h>

#include <stdio.h>
#include <string.h>

#define PRINTF_BUFFER_SIZE 1024

int pstrreadall(pstring_t *out, const char *path) {
    if (!out || !path)
        return PSTRTHROW_EINVAL;

    FILE *file = fopen(path, "r");
    if (!file)
        return PSTRTHROW_EIO;

    fseek(file, 0L, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0L, SEEK_SET);

    if (pstrreserve(out, length)) {
        fclose(file);
        return PSTRTHROW_ENOMEM;
    }

    if (length != fread(pstrend(out), sizeof(char), length, file)) {
        fclose(file);
        return PSTRTHROW_EIO;
    }

    fclose(file);
    pstr__setlen(out, pstrlen(out) + length);
    return PSTRING_OK;
}

int pstrwriteall(const pstring_t *str, const char *path) {
    if (!str || !path)
        return PSTRTHROW_EINVAL;

    FILE *file = fopen(path, "w");
    if (!file)
        return PSTRTHROW_EIO;

    size_t written = fwrite(pstrbuf(str), sizeof(char), pstrlen(str), file);
    fclose(file);

    if (written != pstrlen(str))
        return PSTRTHROW_EIO;

    return PSTRING_OK;
}

int pf_stream_putp(pf_stream_t *stream, const pstring_t *str) {
    if (!stream || !str)
        return PSTRTHROW_EINVAL;
    if (pstrlen(str) == 0)
        return PSTRING_OK;

    size_t written = pf_stream_write(stream, pstrbuf(str), pstrlen(str));
    return pstrlen(str) != written ? PSTRTHROW_EIO : PSTRING_OK;
}

int pstrio_vprintf(pstring_t *dst, const char *fmt, va_list args) {
    if (!dst || !fmt)
        return PSTRTHROW_EINVAL;

    size_t fmtlen = pstr__nlen(fmt, 4096);
    size_t len = fmtlen * 2;
    size_t req;
    va_list copy;

    do {
        req = len + 1;

        if (pstrreserve(dst, req))
            return PSTRTHROW_ENOMEM;

        va_copy(copy, args);
        int result = vsnprintf(pstrend(dst), req, fmt, copy);
        va_end(copy);

        if (result < 0)
            return PSTRTHROW_EIO;

        len = (size_t)result;
    } while (len >= req);

    pstr__setlen(dst, pstrlen(dst) + len);
    return PSTRING_OK;
}

int pstrio_printf(pstring_t *dst, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pstrio_vprintf(dst, fmt, args);
    va_end(args);

    return result;
}

static size_t str_read(pf_stream_t *stream, void *buffer, size_t size) {
    pstring_t *str = stream->state.ptr[0];
    size_t index = (uintptr_t)stream->state.ptr[1];

    if (size > pstrlen(str) - index)
        size = pstrlen(str) - index;

    if (size > 0) {
        memcpy(buffer, &pstrbuf(str)[index], size);
        stream->state.ptr[1] = (void *)(uintptr_t)(index + size);
    }

    return size;
}

static size_t str_write(pf_stream_t *stream, const void *buffer, size_t size) {
    pstring_t *str = stream->state.ptr[0];
    size_t index = (uintptr_t)stream->state.ptr[1];
    size_t left = pstrcap(str) - index;

    if (size > left && pstrreserve(str, size))
        size = left;

    if (size > 0) {
        memcpy(&pstrbuf(str)[index], buffer, size);
        stream->state.ptr[1] = (void *)(uintptr_t)(index + size);

        if (index + size > pstrlen(str))
            pstr__setlen(str, index + size);
    }

    return size;
}

static int str_seek(pf_stream_t *stream, long offset, int origin) {
    pstring_t *str = stream->state.ptr[0];
    size_t index = (uintptr_t)stream->state.ptr[1];
    size_t result;

    switch (origin) {
    case PSTR_SEEK_SET:
        result = offset;
        break;
    case PSTR_SEEK_CUR:
        if (offset < 0 && index < -offset)
            return PSTRING_EINVAL;
        result = index + offset;
        break;
    case PSTR_SEEK_END:
        if (offset < 0 && pstrlen(str) < -offset)
            return PSTRING_EINVAL;
        result = pstrlen(str) + offset;
        break;
    default:
        return PSTRING_EINVAL;
    }

    if (result > pstrlen(str) && pstrreserve(str, result - pstrlen(str)))
        return PSTRING_ENOMEM;

    stream->state.ptr[1] = (void *)(uintptr_t)result;
    return PSTRING_OK;
}

static size_t str_tell(pf_stream_t *stream) {
    return (uintptr_t)stream->state.ptr[1];
}

static void str_flush(pf_stream_t *stream) {
    /* nothing to flush or close */
    return;
}

int pf_stream_pstring(pf_stream_t *out, pstring_t *str) {
    if (!out || !str)
        return PSTRTHROW_EINVAL;

    static const struct pf_stream_vt vtable = {
        .read = str_read,
        .write = str_write,
        .tell = str_tell,
        .seek = str_seek,
        .flush = str_flush,
        .close = str_flush,
    };

    if (pf_stream_init(out, &vtable))
        return PSTRTHROW_ENOSYS;

    out->state.ptr[0] = str;
    out->state.ptr[1] = (void *)(uintptr_t)pstrlen(str);
    return PSTRING_OK;
}

static struct {
    const char *name;
    pstream_save_fn *save;
    pstream_load_fn *load;
} formats[] = {
    { "json", pstream_save_json, pstream_load_json },
    { 0 },
};

static int find_format(const char *name) {
    for (int i = 0; formats[i].name; i++)
        if (0 == strcmp(name, formats[i].name))
            return i;
    return -1;
}

int pstream_save(
    const char *format,
    pf_stream_t *stream,
    const void *obj,
    const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRTHROW_EINVAL;

    int i = find_format(format);

    if (i == -1)
        return PSTRTHROW_ENOSYS;

    int rc = formats[i].save(stream, obj, model);
    return rc ? PSTRTHROW(rc, NULL) : PSTRING_OK;
}

int pstream_load(
    const char *format,
    pf_stream_t *stream,
    void *obj,
    const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRTHROW_EINVAL;

    int i = find_format(format);

    if (i == -1)
        return PSTRTHROW_ENOSYS;

    int rc = formats[i].load(stream, obj, model);
    return rc ? PSTRTHROW(rc, NULL) : PSTRING_OK;
}
