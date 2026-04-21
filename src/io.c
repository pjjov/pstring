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

#define PF_TYPE_HELPERS
#include <pf_macro.h>
#include <pf_typeid.h>
#include <pstring/dictionary.h>
#include <pstring/encoding.h>
#include <pstring/io.h>
#include <pstring/pstring.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

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

int pstream_puts(pstream_t *stream, const char *str) {
    if (!stream || !str)
        return PSTRING_EINVAL;
    size_t length = strlen(str);
    if (length == 0)
        return PSTRING_OK;

    size_t written = pstream_write(stream, str, length);
    return length != written ? PSTRING_EIO : PSTRING_OK;
}

int pstream_putp(pstream_t *stream, const pstring_t *str) {
    if (!stream || !str)
        return PSTRING_EINVAL;
    if (pstrlen(str) == 0)
        return PSTRING_OK;

    size_t written = pstream_write(stream, pstrbuf(str), pstrlen(str));
    return pstrlen(str) != written ? PSTRING_EIO : PSTRING_OK;
}

static int format_next(pstream_t *dst, const char **esc, va_list args);

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

static int format_parse_enc(const char **esc, char *out, int max) {
    const char *end, *start = *esc;

    for (end = start; *end != '%'; end++)
        if (*end == '\0')
            return PSTRING_EINVAL;

    if (end - start >= max)
        return PSTRING_EINVAL;

    memcpy(out, start, end - start);
    out[end - start] = '\0';
    *esc = end;
    return PSTRING_OK;
}

static int format_encoded(pstring_t *dst, const char **esc, va_list args) {
    pstring_t buf = { 0 };
    pstream_t stream;
    char format[32];

    if (format_parse_enc(esc, format, 32))
        return PSTRING_EINVAL;
    pstream_string(&stream, &buf);

    const char *encoding = format;
    if (0 == strcmp(format, "*"))
        encoding = va_arg(args, const char *);

    int result = format_next(&stream, esc, args)
        || pstrenc(dst, &buf, encoding);
    pstrfree(&buf);
    return result;
}

static int format_std(pstream_t *dst, const char *fmt, int len, va_list args) {
    /*
        Calling `vprintf` consumes the `va_list`, causing undefined behaviour.
        To solve this, we call printf by manually passing arguments.
    */

    const char *hasWidth = NULL;
    const char *hasPrec = NULL;

    int width, prec;
    int result = PSTRING_EINVAL;

    if ((hasWidth = strchr(fmt, '*'))) {
        width = va_arg(args, int);

        if ((hasPrec = strchr(hasWidth + 1, '*')))
            prec = va_arg(args, int);
    }

    char type = fmt[len - 1];
    char mod = len >= 2 ? fmt[len - 2] : '\0';
    pf_bool ll = (len >= 3 && fmt[len - 2] == 'l' && fmt[len - 3] == 'l');

    /* clang-format off */

#define PRINTF_2(TYPE) \
    result = pstream__printf(dst, fmt, width, prec, va_arg(args, TYPE))
#define PRINTF_1(TYPE) \
    result = pstream__printf(dst, fmt, width, va_arg(args, TYPE))
#define PRINTF_0(TYPE) \
    result = pstream__printf(dst, fmt, va_arg(args, TYPE))

#define SWITCH(X)                           \
    switch (type) {                         \
    case 'd': case 'i': case 'X':           \
    case 'u': case 'o': case 'x':           \
        if (mod == 'l' && ll) X(long long); \
        else if (mod == 'l')  X(long);      \
        else if (mod == 'j')  X(intmax_t);  \
        else if (mod == 'z')  X(size_t);    \
        else if (mod == 't')  X(ptrdiff_t); \
        else                  X(int);       \
        break;                              \
    case 'f': case 'F':                     \
    case 'e': case 'E':                     \
    case 'g': case 'G':                     \
    case 'a': case 'A':                     \
        if (mod == 'L') X(long double);     \
        else            X(double);          \
        break;                              \
    case 'c':                               \
        if (mod == 'L') X(wint_t);          \
        else            X(int);             \
        break;                              \
    case 's': case 'p': case 'n':           \
        X(void *);                          \
        break;                              \
    }

    /* clang-format on */

    if (hasPrec) {
        SWITCH(PRINTF_2);
    } else if (hasWidth) {
        SWITCH(PRINTF_1);
    } else {
        SWITCH(PRINTF_0);
    }

#undef PRINTF_0
#undef PRINTF_1
#undef PRINTF_2
#undef SWITCH

    return result;
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
        /* todo: use new serialization */
        return PSTRING_ENOSYS;
        // int typeid = va_arg(args, int);
        // void *arg = va_arg(args, void *);
        // return pstream_serialize(dst, typeid, arg);
    }

    case '!': {
        pstring_t enc = { 0 };

        int result = format_encoded(&enc, esc, args);
        if (result == PSTRING_OK)
            pstream_putp(dst, &enc);

        pstrfree(&enc);
        return result;
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
        return format_std(dst, format, len, args);
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

    return pstream_vprintf(&stream, fmt, args);
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

    return pstream_vprintf(&stream, fmt, args);
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
    return fread(buffer, 1, size, file);
}

static size_t file_write(pstream_t *stream, const void *buffer, size_t size) {
    FILE *file = stream->state.ptr[0];
    return fwrite(buffer, 1, size, file);
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
    };

    out->vtable = &vtable;
    out->state.ptr[0] = str;
    out->state.ptr[1] = (void *)(uintptr_t)pstrlen(str);
    return PSTRING_OK;
}

#define JSON_STREAM(x) ((struct json_stream *)&(x)->state._size)
#define JSON_BUFFER_SIZE 4096

struct json_lexer {
    pstream_t *stream;
    size_t start;
    size_t end;
    int eof;
    char buf[JSON_BUFFER_SIZE];
};

struct json_reader {
    struct json_lexer lexer;
    pstream_t *stream;
    int prev, curr;
    unsigned int failed : 1;
    pstring_t prevValue, currValue;
};

struct json_writer {
    pstream_t *base;
    int prev;
};

static int json_serialize_array(
    pstream_t *stream, const void *obj, struct pstrmodel_array *model
) {
    pstream_putc(stream, '[');
    int res = PSTRING_OK;

    for (size_t i = 0; !res && i < model->count; i++) {
        if (i > 0)
            pstream_putc(stream, ',');

        const void *item = PF_OFFSET(obj, model->stride * i);
        res = pstream_save_json(stream, item, model->submodel);
    }

    pstream_putc(stream, ']');
    return res;
}

static int json_serialize_llist(
    pstream_t *stream, const void *obj, struct pstrmodel_llist *model
) {
    pstream_putc(stream, '[');
    int res = PSTRING_OK;

    const void *head = *(const void **)obj;
    const void *item, **link;

    for (item = head; item; item = *link) {
        if (item != head)
            pstream_putc(stream, ',');

        link = PF_OFFSET(item, model->linkOffset);
        res = pstream_save_json(stream, item, model->submodel);
    }

    pstream_putc(stream, ']');
    return res;
}

struct json_serialize_dict_state {
    pstream_t *stream;
    struct pstrmodel *model;
};

static int json_serialize_dict_each(void *user, pstring_t *key, void *value) {
    struct json_serialize_dict_state *state = user;
    return pstream_printf(state->stream, "\"%!json%P\":", key)
        || pstream_save_json(state->stream, value, state->model);
}

static int json_serialize_dict(
    pstream_t *stream, const void *obj, struct pstrmodel *model
) {
    pstream_putc(stream, '{');

    pstrdict_t *dict = *(pstrdict_t **)obj;

    struct json_serialize_dict_state state = { stream, model };
    int res = pstrdict_each(dict, json_serialize_dict_each, &state);

    pstream_putc(stream, '}');
    return res;
}

static int json_serialize(
    struct json_writer *json,
    const struct pstrmodel_member *member,
    const void *item
) {
    pstring_t str;
    int res = PSTRING_OK;

    switch (member->type) {
    case PF_TYPE_BOOL: {
        pf_bool value = *(pf_bool *)item;
        res = pstream_puts(json->base, value ? "true" : "false");
        break;
    }

    case PF_TYPE_CHAR:
        res = pstream__printf(json->base, "\"%c\"", *(char *)item);
        break; /* todo: escape character */
    case PF_TYPE_CSTRING:
        pstrwrap(&str, *(char **)item, 0, 0);
        res = pstream_printf(json->base, "\"%!json%P\"", &str);
        break;
    case PSTRING_TYPE:
        res = pstream_printf(json->base, "\"%!json%P\"", item);
        break;
    case PSTRING_PTR_TYPE:
        res = pstream_printf(json->base, "\"%!json%P\"", *(pstring_t **)item);
        break;

    case PF_TYPE_PTR:
        res = pstream__printf(json->base, "\"%p\"", *(void **)item);
        break;

    case PSTRMODEL_TYPE:
        return pstream_save_json(json->base, item, member->model);
    case PSTRMODEL_ARRAY:
        return json_serialize_array(json->base, item, member->model);
    case PSTRMODEL_LLIST:
        return json_serialize_llist(json->base, item, member->model);
    case PSTRDICT_TYPE:
        return json_serialize_dict(json->base, item, member->model);

    default:
        if (pf_type_is_integer(member->type))
            res = srlz_text_int(json->base, member->type, item);
        else if (pf_type_is_float(member->type))
            res = srlz_text_float(json->base, member->type, item);
        else
            res = PSTRING_ENOSYS;
    }

    json->prev = member->type;
    return res;
}

static int json_write_key(pstream_t *stream, const char *key) {
    pstring_t str;
    pstrwrap(&str, (char *)key, 0, 0);
    return pstream_printf(stream, "\"%!json%P\":", &str);
}

static int json_save_member(
    struct json_writer *json,
    const void *obj,
    const struct pstrmodel_member *member
) {
    const void *item = PF_OFFSET(obj, member->offset);

    if (json->prev != PSTRMODEL__BEGIN && pstream_putc(json->base, ','))
        return PSTRING_EIO;

    if (json_write_key(json->base, member->name))
        return PSTRING_EIO;

    return json_serialize(json, member, item);
}

PSTR_API int pstream_save_json(
    pstream_t *stream, const void *obj, const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRING_EINVAL;

    int result = PSTRING_OK;
    struct json_writer json;
    json.base = stream;
    json.prev = PSTRMODEL__BEGIN;

    result |= pstream_putc(json.base, '{');

    for (size_t i = 0; !result && model->members[i].type; i++)
        result |= json_save_member(&json, obj, &model->members[i]);

    result |= pstream_putc(json.base, '}');

    return result;
}

static int json_isblank(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static int json_reserve(struct json_lexer *lex, size_t count) {
    size_t diff = lex->end - lex->start;

    if (lex->eof || diff >= count)
        return PSTRING_OK;

    if (diff > 0 && lex->start > 0) {
        memmove(lex->buf, &lex->buf[lex->start], diff);
        lex->start = 0;
        lex->end = diff;
    }

    size_t read = pstream_read(
        lex->stream, &lex->buf[lex->end], JSON_BUFFER_SIZE - diff
    );

    if (read == 0)
        lex->eof = PSTRING_TRUE;
    lex->end += read;

    return diff + read < count ? PSTRING_ENOMEM : PSTRING_OK;
}

static int json_skip_blank(struct json_lexer *lex) {
    while (!json_reserve(lex, 1)) {
        if (!json_isblank(lex->buf[lex->start]))
            break;
        lex->start++;
    }

    return lex->eof == 0 ? PSTRING_OK : PSTRING_EIO;
}

static int json_read_number(struct json_lexer *lex) {
    int len = 1;

    while (!json_reserve(lex, len + 1)) {
        char c = lex->buf[lex->start + len];

        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.'
              || c == 'e' || c == 'E'))
            return len;

        len++;
    }

    return lex->eof == 0 ? PSTRING_OK : PSTRING_EIO;
}

static int json_read_string(struct json_lexer *lex, pstring_t *out) {
    int len = 1;
    int escape = PSTRING_FALSE;
    int quote = PSTRING_FALSE;

    while (!json_reserve(lex, len)) {
        char c = lex->buf[lex->start + len];

        if (c == '"' && !escape) {
            quote = PSTRING_TRUE;
            break;
        }

        if (c == '\\' || escape)
            escape = !escape;

        len++;
    }

    if (quote) {
        pstrrange(
            out, NULL, &lex->buf[lex->start + 1], &lex->buf[lex->start + len]
        );

        lex->start += len + 1;
    }

    return lex->eof == 0 || quote ? PSTRING_OK : PSTRING_EIO;
}

static int json_read_keyword(
    struct json_lexer *lex, const char *kw, size_t len
) {
    if (json_reserve(lex, len))
        return PSTRING_EINVAL;

    if (memcmp(kw, &lex->buf[lex->start], len))
        return PSTRING_EINVAL;

    lex->start += len;
    return PSTRING_OK;
}

static int json_read_token(struct json_lexer *lex, pstring_t *out) {
    json_skip_blank(lex);

    if (json_reserve(lex, 1))
        return PSTRING_EIO;

    char c = lex->buf[lex->start];

    switch (c) {
    case '{':
    case '}':
    case '[':
    case ']':
    case ':':
    case ',':
        lex->start++;
        return c;
    case '"':
        if (json_read_string(lex, out))
            return PSTRING_EINVAL;
        return '"';
    case 't':
        if (json_read_keyword(lex, "true", 4))
            return PSTRING_EINVAL;
        return 't';
    case 'f':
        if (json_read_keyword(lex, "false", 5))
            return PSTRING_EINVAL;
        return 'f';
    case 'n':
        if (json_read_keyword(lex, "null", 4))
            return PSTRING_EINVAL;
        return 'n';
    default:
        if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
            int len = json_read_number(lex);

            pstrrange(
                out, NULL, &lex->buf[lex->start], &lex->buf[lex->start + len]
            );

            lex->start += len;
            return 'd';
        }

        return PSTRING_EINVAL;
    }
}

static void json_advance(struct json_reader *json) {
    json->prev = json->curr;
    json->prevValue = json->currValue;
    json->curr = json_read_token(&json->lexer, &json->currValue);
}

static int json_match(struct json_reader *json, int kind) {
    if (json->curr == kind) {
        json_advance(json);
        return PSTRING_TRUE;
    }

    return PSTRING_FALSE;
}

static void json_consume(struct json_reader *json, int kind) {
    if (!json_match(json, kind))
        json->failed = 1;
}

static pf_bool json_expect(struct json_reader *json, int kind) {
    if (!json_match(json, kind)) {
        json->failed = 1;
        return PSTRING_FALSE;
    }

    return PSTRING_TRUE;
}

static struct pstrmodel_member *json_find_member(
    pstring_t *name, const struct pstrmodel *model
) {
    for (size_t i = 0; model->members[i].type; i++)
        if (pstrequals(name, model->members[i].name, 0))
            return &model->members[i];
    return NULL;
}

static void json_skip_value(struct json_reader *json) {
    switch (json->curr) {
    case 'd':
    case 'n':
    case 't':
    case 'f':
    case '"':
        json_advance(json);
        break;

    case '{':
    case '[': {
        char open = json->curr;
        char close = open == '{' ? '}' : ']';

        json_advance(json);
        size_t count = 1;

        while (count > 0) {
            if (json->curr == open)
                count++;
            if (json->curr == close)
                count--;

            json_advance(json);
        }

        break;
    }

    default:
        json->failed = PSTRING_TRUE;
        break;
    }
}

static long double json_consume_number(struct json_reader *json) {
    json_consume(json, 'd');
    return strtold(pstrbuf(&json->prevValue), NULL);
}

static int json_copy_string(struct json_reader *json, pstring_t *out) {
    if (!json_expect(json, '"'))
        return PSTRING_EINVAL;

    if (pstrdec_json(out, &json->prevValue)) {
        json->failed = PSTRING_TRUE;
        return PSTRING_EINVAL;
    }

    return PSTRING_OK;
}

static void json_read_value(
    struct json_reader *json, void *obj, const struct pstrmodel_member *member
) {
    if (json_match(json, 'n'))
        return;

    switch (member->type) {
    case PF_TYPE_CSTRING: {
        pstring_t out = { 0 };

        if (!json_copy_string(json, &out))
            *(char **)obj = pstrunwrap(&out, NULL);
        else
            pstrfree(&out);
        break;
    }

    case PSTRING_TYPE:
        json_copy_string(json, obj);
        break;
    case PSTRING_PTR_TYPE:
        json_copy_string(json, *(pstring_t **)obj);
        break;

    case PSTRMODEL_TYPE:
        pstream_load_json(json->stream, obj, member->model);
        break;

    case PF_TYPE_FLOAT:
        *(float *)obj = json_consume_number(json);
        break;
    case PF_TYPE_DOUBLE:
        *(double *)obj = json_consume_number(json);
        break;
    case PF_TYPE_LDOUBLE:
        *(long double *)obj = json_consume_number(json);
        break;

    case PF_TYPE_CHAR:
    case PF_TYPE_BOOL:
        if (json_match(json, 't') || json_match(json, 'f'))
            *(pf_bool *)obj = (json->prev == 't');
        else
            json->failed = PSTRING_TRUE;
        break;

    default:
        if (pf_type_is_integer(member->type)) {
            uintmax_t value = json_consume_number(json);
            pf_type_int_store(member->type, &value, obj);
            break;
        }

        json_advance(json);
        json->failed = PSTRING_TRUE;
        break;
    }
}

static int json_read_model(
    struct json_reader *json, void *obj, const struct pstrmodel *model
) {
    if (!json_match(json, '{'))
        return PSTRING_EINVAL;

    struct pstrmodel_member *member;
    void *slot;

    while (!json->failed) {
        if (json_match(json, '}'))
            break;

        if (json->prev != '{')
            json_consume(json, ',');

        json_consume(json, '"');
        member = json_find_member(&json->prevValue, model);
        json_consume(json, ':');

        if (member) {
            slot = PF_OFFSET(obj, member->offset);
            json_read_value(json, slot, member);
        } else {
            json_skip_value(json);
        }
    }

    return json->failed;
}

int pstream_load_json(
    pstream_t *stream, void *obj, const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRING_EINVAL;

    struct json_reader json;
    json.failed = 0;
    json.lexer.stream = stream;
    json.lexer.eof = 0;
    json.lexer.start = 0;
    json.lexer.end = 0;

    json_advance(&json);
    int result = json_read_model(&json, obj, model);
    /* seek back */
    return result;
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
    pstream_t *stream,
    const void *obj,
    const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRING_EINVAL;

    int i = find_format(format);
    return i != -1 ? formats[i].save(stream, obj, model) : PSTRING_ENOSYS;
}

int pstream_load(
    const char *format,
    pstream_t *stream,
    void *obj,
    const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRING_EINVAL;

    int i = find_format(format);
    return i != -1 ? formats[i].load(stream, obj, model) : PSTRING_ENOSYS;
}
