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

    do {
        req = len + 1;

        if (pstrreserve(dst, req))
            return PSTRING_ENOMEM;

        int result = vsnprintf(pstrend(dst), req, fmt, args);

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
