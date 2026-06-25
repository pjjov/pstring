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

#include <pstring/pstring.h>

#include <pf_array.h>
#include <pf_ctype.h>

typedef PF_ARRAY(pstring_t) words_t;

typedef int(pstrexpand_fn)(
    void *dst, pstring_t *src, int flags, int kind, void *user
);

typedef struct expand_state_t {
    pstring_t *dst;
    pstring_t *src;
    int flags;
    pstrexpand_fn *cb;
    void *user;
} expand_state_t;

static int is_invalid_char(char c) {
    return c == '|' || c == '&' || c == ';' || c == '<' || c == '>' || c == '{'
        || c == '}' || c == '\n';
}

static int is_escape_char(char c) {
    return c == '$' || c == '`' || c == '"' || c == '\\' || c == '\n';
}

static int check_invalid_chars(pstring_t *src) {
    const char *end = pstrend(src);
    char quote = '\0';

    for (const char *p = pstrbuf(src); p < end; p++) {
        switch (quote) {
        case '\'':
            if (*p == '\'')
                quote = '\0';
            break;
        case '\"':
            if (*p == '\\' && p[1])
                p++;
            else if (*p == '"')
                quote = '\0';
            break;
        default:
            if (*p == '\'')
                quote = '\'';
            else if (*p == '"')
                quote = '"';
            else if (*p == '\\')
                p++;
            else if (is_invalid_char(*p))
                return PSTRING_EINVAL;
            break;
        }
    }

    return PSTRING_OK;
}

static const char *find_closing_brace(const char *p, const char *end) {
    if (!p || !end)
        return NULL;

    char qs = '\0';

    for (int depth = 1; p < end; p++) {
        if (qs == '"' && *p == '\\')
            p++;
        else if (*p == '"')
            qs = (qs == '"') ? '\0' : '"';
        else if (*p == '\'')
            qs = (qs == '\'') ? '\0' : '\'';
        else if (!qs && *p == '$' && &p[1] < end && p[1] == '{')
            depth++;
        else if (!qs && *p == '}' && --depth == 0)
            return p;
    }

    return NULL;
}

static const char *find_closing_paren(const char *p, const char *end) {
    if (!p || !end)
        return NULL;

    char qs = '\0';

    for (int depth = 1; p < end; p++) {
        if (qs == '"' && *p == '\\')
            p++;
        else if (*p == '"')
            qs = (qs == '"') ? '\0' : '"';
        else if (*p == '\'')
            qs = (qs == '\'') ? '\0' : '\'';
        else if (!qs && *p == '(')
            depth++;
        else if (!qs && *p == ')' && --depth == 0)
            return p;
    }

    return NULL;
}

static const char *find_closing_quote(pstring_t *src, char quote) {
    const char *end = pstrend(src);
    const char *curr;
    char search[] = { '\\', quote, '\0' };

    while ((curr = pstrpbrk(src, search))) {
        if (*curr == quote)
            break;
        pstrrange(src, NULL, curr + 2, end);
    }

    if (curr)
        pstrrange(src, NULL, curr, end);
    return curr;
}

static const char *find_closing_double(const char *p, const char *end) {
    if (!p || !end)
        return NULL;

    for (int depth = 1; p < end - 1; p++) {
        if (p[0] == '(' && p[1] == '(') {
            depth++;
            p++;
        } else if (p[0] == ')' && p[1] == ')') {
            if (--depth == 0)
                return p;
            p++;
        }
    }

    return NULL;
}

static int call_handler(expand_state_t *state, int kind, pstring_t *src) {
    return state->cb(state->dst, src, state->flags, kind, state->user);
}