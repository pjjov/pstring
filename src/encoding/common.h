/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef PSTRING_ENCODING_COMMON_H
#define PSTRING_ENCODING_COMMON_H

#include <pstring/core.h>
#include <pstring/encoding.h>
#include <pstring/search.h>

#include <stdint.h>
#include <string.h> /* IWYU pragma: keep */

#define HEXDIGITS "0123456789ABCDEF"

static inline char dec2num(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    return 10;
}

static inline char hex2num(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 17;
}

#endif