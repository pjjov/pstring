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

#define PF_TYPE_HELPERS
#include <pf_typeid.h>
#include <pstring/io.h>
#include <pstring/pstring.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PRINTF_BUFFER_SIZE 1024

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

static char format_parse(const char **esc, char *buffer) {
    const char *start = *esc;
    const char *curr = *esc + 1;
    int precision = PSTRING_FALSE;

    for (char c; (c = *curr); curr++)
        if (c != '#' && c != '-' && c != '+' && c != '0' && c != ' ')
            break;

    while (*curr == '.' || (*curr >= '0' && *curr <= '9')) {
        if (*curr == '.') {
            if (precision)
                break;
            precision = PSTRING_TRUE;
            curr++;
        }

        curr++;
    }

    switch (*curr) {
    case 'h':
    case 'l':
        if (curr[1] == curr[0])
            curr++;
    case 'L':
    case 'z':
    case 'j':
    case 't':
        curr++;
    default:
        break;
    }

    if (*curr)
        curr++;

    if (curr - start < 32) {
        memcpy(buffer, start, curr - start);
        buffer[curr - start] = '\0';
    } else {
        buffer[0] = '\0';
    }

    *esc = curr;
    return curr - start;
}

static int format_unsigned(unsigned long long *value, char chr, va_list args) {
    uintmax_t max;

    switch (chr) {
        /* clang-format off */
    case 'b': max = (uint8_t)va_arg(args, unsigned int);  break;
    case 'w': max = (uint16_t)va_arg(args, unsigned int); break;
    case 'd': max = va_arg(args, uint32_t);               break;
    case 'q': max = va_arg(args, uint64_t);               break;
    case 'm': max = va_arg(args, uintmax_t);              break;
    case 'p': max = va_arg(args, uintptr_t);              break;
    case 's': max = va_arg(args, size_t);                 break;
    default: return PSTRING_EINVAL;
        /* clang-format on */
    }

    if (max > ULLONG_MAX)
        return PSTRING_ERANGE;

    *value = max;
    return PSTRING_OK;
}

static int format_signed(long long *value, char chr, va_list args) {
    intmax_t max;

    switch (chr) {
        /* clang-format off */
    case 'b': max = (int8_t)va_arg(args, int);  break;
    case 'w': max = (int16_t)va_arg(args, int); break;
    case 'd': max = va_arg(args, int32_t);      break;
    case 'q': max = va_arg(args, int64_t);      break;
    case 'm': max = va_arg(args, intmax_t);     break;
    case 'p': max = va_arg(args, intptr_t);     break;
    case 'P': max = va_arg(args, ptrdiff_t);    break;
    default: return PSTRING_EINVAL;
        /* clang-format on */
    }

    if (max > LLONG_MAX || max < LLONG_MIN)
        return PSTRING_ERANGE;

    *value = max;
    return PSTRING_OK;
}

static int format_next(pstream_t *dst, const char **esc, va_list args) {
    char format[32];
    int len = format_parse(esc, format);

    switch (format[len - 1]) {
    case 'P': {
        pstring_t *arg = va_arg(args, pstring_t *);
        size_t written = pstream_write(dst, pstrbuf(arg), pstrlen(arg));
        return written == pstrlen(arg) ? PSTRING_OK : PSTRING_EIO;
    }

    case '?': {
        int typeid = va_arg(args, int);
        void *arg = va_arg(args, void *);
        return pstream_serialize(dst, typeid, arg);
    }

    case 'D': {
        const char *fmt = va_arg(args, const char *);
        struct tm *tp = va_arg(args, struct tm *);

        char buffer[256];
        if (0 == strftime(buffer, 256, fmt, tp))
            return PSTRING_EINVAL;

        size_t fmtlen = pstr__nlen(buffer, 256);
        size_t written = pstream_write(dst, buffer, fmtlen);
        return written == fmtlen ? PSTRING_OK : PSTRING_EIO;
    }

    case 'U': {
        if (strchr(format, '*'))
            return PSTRING_EINVAL;

        unsigned long long value;
        if (format_unsigned(&value, *(*esc)++, args))
            return PSTRING_EINVAL;

        format[len - 1] = 'l';
        format[len] = 'l';
        format[len + 1] = 'u';
        return pstream__printf(dst, format, value);
    }

    case 'I': {
        if (strchr(format, '*'))
            return PSTRING_EINVAL;

        long long value;
        if (format_signed(&value, *(*esc)++, args))
            return PSTRING_EINVAL;

        format[len - 1] = 'l';
        format[len] = 'l';
        format[len + 1] = 'd';
        return pstream__printf(dst, format, value);
    }

    default:
        return pstream__vprintf(dst, format, args);
    }

    return PSTRING_OK;
}

int pstrfmtv(pstring_t *dst, const char *fmt, va_list args) {
    if (!dst || !fmt)
        return PSTRING_EINVAL;

    pstream_t stream;
    if (pstream_string(&stream, dst))
        return PSTRING_EINVAL;

    size_t original = pstrlen(dst);
    int result;

    if ((result = pstream_vprintf(&stream, fmt, args))) {
        pstr__setlen(dst, original);
        return result;
    }

    return PSTRING_OK;
}

int pstrfmt(pstring_t *dst, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pstrfmtv(dst, fmt, args);
    va_end(args);
    return result;
}

int pstrprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pstrvprintf(fmt, args);
    va_end(args);
    return result;
}

int pstrvprintf(const char *fmt, va_list args) {
    pstream_t stream;

    if (pstream_file(&stream, stdout))
        return PSTRING_EINVAL;

    return pstream_printf(&stream, fmt, args);
}

int pstrerrorf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pstrverrorf(fmt, args);
    va_end(args);
    return result;
}

int pstrverrorf(const char *fmt, va_list args) {
    pstream_t stream;

    if (pstream_file(&stream, stderr))
        return PSTRING_EINVAL;

    return pstream_printf(&stream, fmt, args);
}

int pstream_printf(pstream_t *stream, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pstream_vprintf(stream, fmt, args);
    va_end(args);
    return result;
}

int pstream_vprintf(pstream_t *stream, const char *fmt, va_list args) {
    if (!stream || !fmt)
        return PSTRING_EINVAL;

    const char *prev = fmt;
    const char *match = fmt;
    while ((match = strchr(prev, '%'))) {
        pstream_write(stream, prev, match - prev);

        if (format_next(stream, &match, args))
            return PSTRING_EINVAL;

        prev = match;
    }

    pstream_write(stream, prev, strlen(prev));
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

int pstream__printf(pstream_t *stream, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int res = pstream__vprintf(stream, fmt, args);
    va_end(args);

    return res;
}

int pstream__vprintf(pstream_t *stream, const char *fmt, va_list args) {
    if (!stream || !fmt)
        return PSTRING_EINVAL;

    char buffer[PRINTF_BUFFER_SIZE];

    int res = vsnprintf(buffer, PRINTF_BUFFER_SIZE, fmt, args);

    if (res >= PRINTF_BUFFER_SIZE)
        return PSTRING_ENOMEM;

    if (res < 0 || res != pstream_write(stream, buffer, res))
        return PSTRING_EIO;

    return PSTRING_OK;
}

static int srlz_text_int(pstream_t *stream, int type, const void *item) {
    char buffer[256];
    uintmax_t value;
    int res;

    if (pf_type_int_load(type, item, &value))
        return PSTRING_EINVAL;

    if (pf_type_is_unsigned(type))
        res = snprintf(buffer, 256, "%llu", (unsigned long long)value);
    else
        res = snprintf(buffer, 256, "%lld", (long long)value);

    if (res <= 0 || res >= 256)
        return PSTRING_EINVAL;

    if (res != pstream_write(stream, buffer, res))
        return PSTRING_EIO;

    return PSTRING_OK;
}

static int srlz_text_float(pstream_t *stream, int type, const void *item) {
    char buffer[256];
    double value;
    int res;

    switch (type) {
        /* clang-format off */
    case PF_TYPE_FLOAT:   value = *(float *)item; break;
    case PF_TYPE_DOUBLE:  value = *(double *)item; break;
    case PF_TYPE_LDOUBLE: value = *(long double *)item; break;
    default: return PSTRING_EINVAL;
        /* clang-format on */
    }

    res = snprintf(buffer, 256, "%f", value);
    if (res <= 0 || res >= 256)
        return PSTRING_EINVAL;

    if (res != pstream_write(stream, buffer, res))
        return PSTRING_EIO;

    return PSTRING_OK;
}

static int srlz_text(pstream_t *stream, int type, const void *item) {
    if (pf_type_is_integer(type))
        return srlz_text_int(stream, type, item);
    if (pf_type_is_float(type))
        return srlz_text_float(stream, type, item);
    if (type == PF_TYPE_CHAR)
        return 1 != pstream_write(stream, item, 1);
    if (type == PF_TYPE_PTR)
        return pstream__printf(stream, "%p", *(void **)item);

    if (type == PF_TYPE_CSTRING) {
        size_t length = strlen(item);
        return length != pstream_write(stream, item, length);
    }

    return PSTRING_EINVAL;
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

static size_t file_write(pstream_t *stream, const void *buffer, size_t size) {
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
    /* todo: binary mode serialization */
    return srlz_text(stream, type, item);
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
        .serialize = file_serialize,
        .deserialize = file_deserialize,
    };

    out->vtable = &vtable;
    out->state.ptr[0] = file;
    return PSTRING_OK;
}

static size_t str_read(pstream_t *stream, void *buffer, size_t size) {
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

static size_t str_write(pstream_t *stream, const void *buffer, size_t size) {
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

static int str_seek(pstream_t *stream, long offset, int origin) {
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

static size_t str_tell(pstream_t *stream) {
    return (uintptr_t)stream->state.ptr[1];
}

static void str_flush(pstream_t *stream) {
    /* nothing to flush or close */
    return;
}

static int str_deserialize(pstream_t *stream, int type, void *item) {
    return PSTRING_ENOSYS;
}

int pstream_string(pstream_t *out, pstring_t *str) {
    if (!out || !str)
        return PSTRING_EINVAL;

    static const struct pstream_vt vtable = {
        .read = str_read,
        .write = str_write,
        .tell = str_tell,
        .seek = str_seek,
        .flush = str_flush,
        .close = str_flush,
        .serialize = srlz_text,
        .deserialize = str_deserialize,
    };

    out->vtable = &vtable;
    out->state.ptr[0] = str;
    out->state.ptr[1] = (void *)(uintptr_t)pstrlen(str);
    return PSTRING_OK;
}
