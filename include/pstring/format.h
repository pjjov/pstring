/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_FORMAT_H
#define PSTRING_FORMAT_H

/** Module: String formatting functions

    This module provides time and string formatting functions.

    Alongside standard formatting options, formatting functions offer extensions
    for commonly used types that are missing from the standard library and
    additional encoding support:

    - `%P` - prints the passed `pstring_t *` argument.
    - `%D` - prints calendar time using the passed format.
    - `%?` - serializes a pointer to a type indicated by the passed `int` id.
    - `%!` - encodes the following format option using the specified encoding.
    - `%Ib`, `%Ub` - prints `int8_t` and `uint8_t` respectively.
    - `%Iw`, `%Uw` - prints `int16_t` and `uint16_t` respectively.
    - `%Id`, `%Ud` - prints `int32_t` and `uint32_t` respectively.
    - `%Iq`, `%Uq` - prints `int64_t` and `uint64_t` respectively.
    - `%Im`, `%Um` - prints `intmax_t` and `uintmax_t` respectively.
    - `%Ip`, `%Up` - prints `intptr_t` and `uintptr_t` respectively.
    - `%IP`, `%Us` - prints `ptrdiff_t` and `size_t` respectively.

    ```c
        pstring_t *str, *other;
        pstrfmt(str, "%P", other);

        int num;
        pstrfmt(str, "%?", PF_TYPE_INT, &num);

        struct tm date = { .tm_mday = 30 };
        pstrfmt(str, "%D", "%A %c", &date);

        pstrfmt(str, "%!html%P world!", PSTR("<Hello>"));

        pstrfmt(str, "Bye %!*%s!", "hex", "world");
    ```

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
*/

#ifndef PSTR_API
    #define PSTR_API
#endif

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct pstring_t pstring_t;
typedef struct pf_stream_t pf_stream_t;
struct tm;

/** Concatenates a string formatted according to `fmt`.

    > This function supports formatting extensions.

    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
*/
PSTR_API int pstrfmt(pstring_t *dst, const char *fmt, ...);
PSTR_API int pstrfmtv(pstring_t *dst, const char *fmt, va_list args);

/** Prints a formatted string to standard output.

    > This function supports formatting extensions.

    Possible error codes: PSTRING_EINVAL, <stdio.h> error codes.
*/
PSTR_API int pstrprintf(const char *fmt, ...);
PSTR_API int pstrvprintf(const char *fmt, va_list args);

/** Prints a formatted string to standard error output.

    > This function supports formatting extensions.

    Possible error codes: PSTRING_EINVAL, <stdio.h> error codes.
*/
PSTR_API int pstrerrorf(const char *fmt, ...);
PSTR_API int pstrverrorf(const char *fmt, va_list args);

/** Prints a formatted string to given character stream.

    > This function supports formatting extensions.

    Possible error codes: PSTRING_EINVAL, stream error codes.
*/
PSTR_API int pstrfprintf(pf_stream_t *stream, const char *fmt, ...);
PSTR_API int pstrvfprintf(pf_stream_t *stream, const char *fmt, va_list args);

/** Concatenates a string formatted by `fmt` using date and time from `src`.
    Possible error codes: PSTRING_EINVAL, PSTRING_ENOMEM.
*/
PSTR_API int pstrftime(pstring_t *dst, const char *fmt, struct tm *src);

#ifdef __cplusplus
}
#endif

#endif /* PSTRING_FORMAT_H */