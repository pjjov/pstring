/*
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

#include <pstring/io.h>
#include <pstring/pstring.h>
#include <stdarg.h>
#include <stdio.h>

int pstrread(pstring_t *out, const char *path) {
    if (!out || !path)
        return PSTRING_EINVAL;

    FILE *file = fopen(path, "r");
    if (!file)
        return PSTRING_EIO;

    fseek(file, 0L, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0L, SEEK_SET);

    if (pstrreserve(out, length)) {
        fclose(file);
        return PSTRING_ENOMEM;
    }

    if (length != fread(pstrend(out), sizeof(char), length, file)) {
        fclose(file);
        return PSTRING_EIO;
    }

    fclose(file);
    pstr__setlen(out, pstrlen(out) + length);
    return PSTRING_OK;
}

int pstrwrite(const pstring_t *str, const char *path) {
    if (!str || !path)
        return PSTRING_EINVAL;

    FILE *file = fopen(path, "w");
    if (!file)
        return PSTRING_EIO;

    size_t written = fwrite(pstrbuf(str), sizeof(char), pstrlen(str), file);
    fclose(file);

    if (written != pstrlen(str))
        return PSTRING_EIO;

    return PSTRING_OK;
}

int pstrio_vprintf(pstring_t *dst, const char *fmt, va_list args) {
    if (!dst || !fmt)
        return PSTRING_EINVAL;

    size_t fmtlen = pstr__nlen(fmt, 4096);
    size_t len = fmtlen * 2;
    size_t req;
    va_list copy;

    do {
        req = len + 1;

        if (pstrreserve(dst, req))
            return PSTRING_ENOMEM;

        va_copy(copy, args);
        int result = vsnprintf(pstrend(dst), req, fmt, copy);
        va_end(copy);

        if (result < 0)
            return PSTRING_EIO;

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

int pstream_init(pstream_t *out, const struct pstream_vt *vtable) {
    if (!out || !vtable)
        return PSTRING_EINVAL;

    int fail = PSTRING_FALSE;
    fail |= !vtable->read;
    fail |= !vtable->write;
    fail |= !vtable->tell;
    fail |= !vtable->seek;
    fail |= !vtable->flush;
    fail |= !vtable->close;
    fail |= !vtable->serialize;
    fail |= !vtable->deserialize;

    out->vtable = vtable;
    return fail ? PSTRING_EINVAL : PSTRING_OK;
}

int pstream_open(pstream_t *out, const char *path, const char *mode) {
    if (!out || !path || !mode)
        return PSTRING_EINVAL;

    FILE *file = fopen(path, mode);
    if (pstream_file(out, file))
        return PSTRING_EIO;

    return PSTRING_OK;
}

static size_t file_read(pstream_t *stream, void *buffer, size_t size) {
    FILE *file = stream->state.ptr[0];
    return fread(buffer, size, 1, file);
}

static size_t file_write(pstream_t *stream, void *buffer, size_t size) {
    FILE *file = stream->state.ptr[0];
    return fwrite(buffer, size, 1, file);
}

static int file_seek(pstream_t *stream, long offset, int origin) {
    FILE *file = stream->state.ptr[0];
    return fseek(file, offset, origin);
}

static size_t file_tell(pstream_t *stream) {
    FILE *file = stream->state.ptr[0];
    return ftell(file);
}

static void file_flush(pstream_t *stream) {
    FILE *file = stream->state.ptr[0];
    fflush(file);
}

static void file_close(pstream_t *stream) {
    FILE *file = stream->state.ptr[0];
    fclose(file);
}

static int file_serialize(pstream_t *stream, int type, const void *item) {
    return PSTRING_ENOSYS;
}

static int file_deserialize(pstream_t *stream, int type, void *item) {
    return PSTRING_ENOSYS;
}

int pstream_file(pstream_t *out, FILE *file) {
    if (!out || !file)
        return PSTRING_EINVAL;

    static const struct pstream_vt vtable = {
        .read = file_read,
        .write = file_write,
        .tell = file_tell,
        .seek = file_seek,
        .flush = file_flush,
        .close = file_close,
    };

    out->vtable = &vtable;
    return PSTRING_OK;
}
