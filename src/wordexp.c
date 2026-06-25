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

static int expand_tilde(expand_state_t *state) {
    return call_handler(state, PSTREXPAND_TILDE, NULL);
}

static int expand_single_quote(pstring_t *dst, pstring_t *src) {
    if (pstrget(src, 0) == '\'')
        return PSTRING_OK;

    const char *start = pstrbuf(src);
    const char *end = pstrend(src);

    const char *quote = pstrchr(src, '\'');

    if (quote > start)
        pstrcats(src, start, quote - start);
    pstrrange(src, NULL, quote ? quote + 1 : end, end);
    return PSTRING_OK;
}

static int expand_double_quote(pstring_t *dst, pstring_t *src) {
    if (pstrget(src, 0) == '"')
        return PSTRING_OK;

    const char *start = pstrbuf(src);
    const char *close = find_closing_quote(src, '"');

    if (!close)
        return PSTRING_EINVAL;

    /* TODO: decode and concat */
    return PSTRING_ENOSYS;
}

static int expand_escape(pstring_t *dst, pstring_t *src) {
    if (pstrlen(src) == 0)
        return PSTRING_OK;

    char c = pstrget(src, 0);
    if (c != '\n')
        pstrcatc(dst, c);

    pstrrshift(src, 1);
    return PSTRING_OK;
}

static int expand_command_tick(expand_state_t *state) {
    if (pstrget(state->src, 0) == '`')
        return PSTRING_EINVAL;

    const char *start = pstrbuf(state->src);
    const char *close = find_closing_quote(state->src, '`');

    if (!close)
        return PSTRING_EINVAL;

    pstring_t cmd;
    pstrrange(&cmd, NULL, start, close);
    return call_handler(state, PSTREXPAND_CMD_TICK, &cmd);
}

static int expand_command_paren(expand_state_t *state) {
    pstring_t *src = state->src;
    const char *open = pstrslot(src, 1);
    const char *close = find_closing_paren(open, pstrend(src));

    if (!close)
        return PSTRING_EINVAL;

    pstring_t var;
    pstrrange(&var, NULL, open, close);
    pstrrange(src, NULL, close + 1, pstrend(src));

    return call_handler(state, PSTREXPAND_CMD_PAREN, &var);
}

static int expand_braced(expand_state_t *state) {
    pstring_t *src = state->src;
    const char *open = pstrslot(src, 1);
    const char *close = find_closing_brace(open, pstrend(src));

    if (!close)
        return PSTRING_EINVAL;

    pstring_t var;
    pstrrange(&var, NULL, open, close);
    pstrrange(src, NULL, close + 1, pstrend(src));

    return call_handler(state, PSTREXPAND_BRACE, &var);
}

static int expand_arithmetic(expand_state_t *state) {
    pstring_t *src = state->src;
    const char *open = pstrslot(src, 2);
    const char *close = find_closing_double(open, pstrend(src));

    if (!close)
        return PSTRING_EINVAL;

    pstring_t var;
    pstrrange(&var, NULL, open, close);
    pstrrange(src, NULL, close + 2, pstrend(src));

    return call_handler(state, PSTREXPAND_ARITHMETIC, &var);
}

static int expand_named(expand_state_t *state) {
    pstring_t *src = state->src;
    const char *start = pstrbuf(src);
    const char *end;

    if (!(end = pstrcpbrk(src, PF_CTYPE_ALNUM "_")))
        end = pstrend(src);

    pstring_t var;
    pstrrange(&var, NULL, start, end);
    pstrrange(src, NULL, end, pstrend(src));

    int rc = call_handler(state, PSTREXPAND_NAMED, &var);
    return rc;
}

static int expand_pid(expand_state_t *state) {
    pstrrshift(state->src, 1);
    return call_handler(state, PSTREXPAND_PID, NULL);
}

static int expand_status(expand_state_t *state) {
    pstrrshift(state->src, 1);
    return call_handler(state, PSTREXPAND_STATUS, NULL);
}

static int expand_string(expand_state_t *state) {
    pstring_t *dst = state->dst;
    pstring_t *src = state->src;

    const char *start = pstrbuf(src);
    const char *special, *prev = start;
    const char *end = pstrend(src);
    int res = PSTRING_OK;

    while (!res && (special = pstrpbrk(src, "$`'\\\"~"))) {
        if ((res = pstrcatb(dst, prev, special - prev)))
            break;
        pstrrange(src, NULL, special + 1, end);

        switch (*special) {
        case '\\':
            res = expand_escape(dst, src);
            break;
        case '"':
            res = expand_double_quote(dst, src);
            break;
        case '\'':
            res = expand_single_quote(dst, src);
            break;
        case '`':
            res = expand_command_tick(state);
            break;
        case '$': {
            char curr = pstrget(src, 1);
            char next = pstrget(src, 2);

            if (curr == '(' && next == '(')
                res = expand_arithmetic(state);
            else if (curr == '(')
                res = expand_command_paren(state);
            else if (curr == '{')
                res = expand_braced(state);
            else if (pf_isalpha(curr))
                res = expand_named(state);
            else if (curr == '$')
                res = expand_pid(state);
            else if (curr == '?')
                res = expand_status(state);
            else
                res = PSTRING_EINVAL;
            break;
        }
        case '~':
            if (special == start || special[-1] == ':' || special[-1] == '=')
                res = expand_tilde(state);
            else
                pstrcatc(dst, '~');
            break;
        default:
            break;
        }

        prev = pstrbuf(src);
    }

    if (!res && end > prev)
        res = pstrcats(dst, prev, end - prev);
    return res;
}
