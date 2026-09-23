/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/core.h>
#include <pstring/encoding.h>
#include <pstring/format.h>
#include <pstring/io.h>

#include <pf_io.h>
#include <wchar.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

int pstrftime(pstring_t *dst, const char *fmt, struct tm *src) {
    if (!dst || !fmt || !src)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, strlen(fmt) * 2))
        return PSTRING_ENOMEM;

    size_t space = pstrcap(dst) - pstrlen(dst);
    size_t written = strftime(pstrend(dst), space, fmt, src);
    pstr__setlen(dst, pstrlen(dst) + written);
    return written > 0 ? PSTRING_OK : PSTRTHROW_ENOMEM;
}

static int format_next(pf_stream_t *dst, const char **esc, va_list args);

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
    pf_stream_t stream;
    char format[32];

    if (format_parse_enc(esc, format, 32))
        return PSTRING_EINVAL;
    pf_stream_pstring(&stream, &buf);

    const char *encoding = format;
    if (0 == strcmp(format, "*"))
        encoding = va_arg(args, const char *);

    int result = format_next(&stream, esc, args)
        || pstrenc(dst, &buf, encoding);
    pstrfree(&buf);
    return result;
}

static int format_std(
    pf_stream_t *dst, const char *fmt, int len, va_list args
) {
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
    int ll = (len >= 3 && fmt[len - 2] == 'l' && fmt[len - 3] == 'l');

    /* clang-format off */

#define PRINTF_2(TYPE) \
    result = pf_stream_printf(dst, fmt, width, prec, va_arg(args, TYPE))
#define PRINTF_1(TYPE) \
    result = pf_stream_printf(dst, fmt, width, va_arg(args, TYPE))
#define PRINTF_0(TYPE) \
    result = pf_stream_printf(dst, fmt, va_arg(args, TYPE))

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

static int format_next(pf_stream_t *dst, const char **esc, va_list args) {
    char format[32];
    int len = format_parse(esc, format);

    switch (format[len - 1]) {
    case 'P': {
        pstring_t *arg = va_arg(args, pstring_t *);
        size_t written = pf_stream_write(dst, pstrbuf(arg), pstrlen(arg));
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
            pf_stream_putp(dst, &enc);

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
        size_t written = pf_stream_write(dst, buffer, fmtlen);
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
        return pf_stream_printf(dst, format, value);
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
        return pf_stream_printf(dst, format, value);
    }

    default:
        return format_std(dst, format, len, args);
    }

    return PSTRING_OK;
}

int pstrfmtv(pstring_t *dst, const char *fmt, va_list args) {
    if (!dst || !fmt)
        return PSTRTHROW_EINVAL;

    pf_stream_t stream;
    if (pf_stream_pstring(&stream, dst))
        return PSTRTHROW_EINVAL;

    size_t original = pstrlen(dst);
    int rc;

    if ((rc = pstrvfprintf(&stream, fmt, args))) {
        pstr__setlen(dst, original);
        return PSTRTHROW(rc, NULL);
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
    pf_stream_t stream;

    if (pf_stream_file(&stream, stdout))
        return PSTRTHROW_EINVAL;

    return pstrvfprintf(&stream, fmt, args);
}

int pstrerrorf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pstrverrorf(fmt, args);
    va_end(args);
    return result;
}

int pstrverrorf(const char *fmt, va_list args) {
    pf_stream_t stream;

    if (pf_stream_file(&stream, stderr))
        return PSTRTHROW_EINVAL;

    return pstrvfprintf(&stream, fmt, args);
}

int pstrfprintf(pf_stream_t *stream, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pstrvfprintf(stream, fmt, args);
    va_end(args);
    return result;
}

int pstrvfprintf(pf_stream_t *stream, const char *fmt, va_list args) {
    if (!stream || !fmt)
        return PSTRTHROW_EINVAL;

    const char *prev = fmt;
    const char *match = fmt;
    while ((match = strchr(prev, '%'))) {
        pf_stream_write(stream, prev, match - prev);

        if (format_next(stream, &match, args))
            return PSTRTHROW_EINVAL;

        prev = match;
    }

    pf_stream_write(stream, prev, strlen(prev));
    return PSTRING_OK;
}