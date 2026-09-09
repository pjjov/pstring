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

#include "allocator_std.h"
#include <pstring/pstring.h>

#include <stdalign.h>

#include <allocator.h>
#include <allocator_arena.h>
#include <pf_ctype.h>

#define PF_ARRAY_USE_ALLOCATOR_T
#define PF_ARRAY_DEFAULT_ALLOCATOR NULL
#include <pf_array.h>

typedef PF_ARRAY(pstring_t) words_t;

typedef struct pstrexpand_t {
    int flags;
    pstrexpand_fn *cb;
    void *user;
} pstrexpand_t;

typedef struct expand_state_t {
    pstring_t *dst;
    pstring_t *src;

    pstrexpand_t *handler;
} expand_state_t;

typedef struct split_state_t {
    pstring_t *src;
    words_t *words;
    pstring_t ifs;

    char ws[64];
    char nws[64];

    pstrexpand_t *handler;
} split_state_t;

typedef struct path_state_t {
    words_t *words;
    words_t *result;

    pstrexpand_t *handler;
} path_state_t;

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

static int words_push_dup(words_t *words, const pstring_t *s) {
    pstring_t *dst;

    if (!(dst = PF_ARRAY_INCR(words, 1)))
        return PSTRING_ENOMEM;

    if (pstrdup(dst, s, NULL)) {
        PF_ARRAY_DECR(words, 1);
        return PSTRING_ENOMEM;
    }

    return PSTRING_OK;
}

static int call_handler(pstrexpand_t *handler, int kind, void *src, void *dst) {
    return handler->cb(dst, src, handler->flags, kind, handler->user);
}

static int expand_tilde(expand_state_t *state) {
    return call_handler(state->handler, PSTREXPAND_TILDE, NULL, state->dst);
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
    return call_handler(state->handler, PSTREXPAND_CMD_TICK, &cmd, state->dst);
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

    return call_handler(state->handler, PSTREXPAND_CMD_PAREN, &var, state->dst);
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

    return call_handler(state->handler, PSTREXPAND_BRACE, &var, state->dst);
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

    return call_handler(
        state->handler, PSTREXPAND_ARITHMETIC, &var, state->dst
    );
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

    int rc = call_handler(state->handler, PSTREXPAND_NAMED, &var, state->dst);
    return rc;
}

static int expand_pid(expand_state_t *state) {
    pstrrshift(state->src, 1);
    return call_handler(state->handler, PSTREXPAND_PID, NULL, state->dst);
}

static int expand_status(expand_state_t *state) {
    pstrrshift(state->src, 1);
    return call_handler(state->handler, PSTREXPAND_STATUS, NULL, state->dst);
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

static int init_ifs_param(split_state_t *state) {
    int rc = call_handler(state->handler, PSTREXPAND_IFS, NULL, &state->ifs);

    if (rc == PSTRING_ENOENT) {
        pstrwrap(&state->ifs, " \t\n", 3, 0);
        return 0;
    }

    return rc;
}

static int init_ifs_tables(split_state_t *state) {
    if (init_ifs_param(state))
        return PSTRING_EINVAL;

    pstring_t ws, nws;

    pstrwrapb(&ws, state->ws, 0, 63);
    pstrwrapb(&nws, state->nws, 0, 63);

    for (const char *c = pstrbuf(&state->ifs); *c; c++)
        if (pstrcatc(pf_isspace(*c) ? &ws : &nws, *c))
            break;

    state->ws[pstrlen(&ws)] = '\0';
    state->nws[pstrlen(&nws)] = '\0';
    return PSTRING_OK;
}

static int fs_skip_ws(split_state_t *state) {
    pstring_t *src = state->src;

    if (strchr(state->nws, pstrget(src, 0))) {
        pstrrshift(src, 1);
        pstrlstrip(src, state->ws);

        if (pstrlen(src) == 0) {
            pstring_t empty;
            pstrslice(&empty, src, 0, 0);
            return PF_ARRAY_PUSH(state->words, &empty, 1);
        }
    } else {
        pstrlstrip(src, state->ws);
    }

    return PSTRING_OK;
}

static int field_split(split_state_t *state) {
    if (init_ifs_tables(state)) /* empty IFS */
        return words_push_dup(state->words, state->src);

    pstrlstrip(state->src, state->ws);

    pstring_t field;
    pstring_t *src = state->src;
    const char *ws = pstrbuf(state->src);
    int rc = PSTRING_OK;

    while (!rc && ws < pstrend(src)) {
        if (!(ws = pstrcpbrk(src, pstrbuf(&state->ifs))))
            ws = pstrend(src);

        pstrrange(&field, NULL, pstrbuf(src), ws);
        pstrrange(src, NULL, ws, pstrend(src));

        pstrrstrip(&field, state->ws);
        rc = words_push_dup(state->words, &field) || fs_skip_ws(state);
    }

    return rc;
}

static int quote_remove(pstring_t *dst, const pstring_t *src) {
    const char *s, *end = pstrend(src);
    char quote = '\0';
    int rc = PSTRING_OK;

    for (s = pstrbuf(src); !rc && s < end; s++) {
        if (quote != '"' && *s == '\'')
            quote = quote == '\'' ? '\0' : '\'';
        else if (quote != '\'' && *s == '\"')
            quote = quote == '\"' ? '\0' : '\"';
        else if (quote != '\'' && *s == '\\' && &s[1] < end)
            rc = pstrcatc(dst, *(++s));
        else
            rc = pstrcatc(dst, *s);
    }

    return rc;
}

static int expand_pathname(path_state_t *state) {
    pstring_t clean = { 0 };

    for (size_t i = 0; i < PF_ARRAY_LEN(state->words); i++) {
        if (quote_remove(&clean, PF_ARRAY_SLOT(state->words, i)))
            return PSTRING_EINVAL;

        int hasGlob = pstrpbrk(&clean, "*?[") != NULL;

        if (hasGlob) {
            int rc = call_handler(
                state->handler, PSTREXPAND_GLOB, &clean, &state->result
            );

            pstrfree(&clean);
            clean = (pstring_t) { 0 };

            if (rc != PSTRING_OK)
                return rc;
        } else {
            PF_ARRAY_PUSH(state->result, &clean, 1);
            clean = (pstring_t) { 0 };
        }
    }

    return PSTRING_OK;
}

static void words_free(words_t *words) {
    for (size_t i = 0; i < PF_ARRAY_LEN(words); i++)
        pstrfree(PF_ARRAY_SLOT(words, i));
    PF_ARRAY_FREE(words);
}

static int expand_with(
    pstrexpand_t *handler, allocator_t *arena, words_t *result, pstring_t *src
) {
    pstring_t expanded = { 0 };
    if (pstralloc(&expanded, 4096, arena))
        return PSTRING_ENOMEM;

    pstring_t srcSlice;
    pstrslice(&srcSlice, src, 0, pstrlen(src));

    expand_state_t expandState;
    expandState.handler = handler;
    expandState.dst = &expanded;
    expandState.src = &srcSlice;

    int rc;

    if ((rc = expand_string(&expandState)))
        return rc;

    pstring_t expandedSlice;
    pstrslice(&expandedSlice, &expanded, 0, pstrlen(&expanded));

    words_t splitWords;
    PF_ARRAY_WITH_ALLOCATOR(&splitWords, arena);

    split_state_t splitState;
    splitState.handler = handler;
    splitState.src = &expandedSlice;
    splitState.words = &splitWords;

    if ((rc = field_split(&splitState)))
        return rc;

    path_state_t pathState;
    pathState.handler = handler;
    pathState.words = &splitWords;
    pathState.result = result;

    if ((rc = expand_pathname(&pathState)))
        return rc;

    return PSTRING_OK;
}

int pstrexpand_with(
    pstrarray_t *dst, pstring_t *src, int flags, pstrexpand_fn *cb, void *user
) {
    if (!dst || !src || !cb || check_invalid_chars(src))
        return PSTRTHROW_EINVAL;

    pstrexpand_t handler;
    handler.cb = cb;
    handler.user = user;
    handler.flags = flags;

    char _arenaBuffer[4096];
    struct arena_alloc _arena = { 0 };
    arena_alloc_init(&_arena, &standard_allocator);
    arena_alloc_buffer(&_arena, _arenaBuffer, 4096);
    allocator_t *arena = &_arena.alloc;

    words_t words;
    PF_ARRAY_WITH_ALLOCATOR(&words, &standard_allocator);

    int rc = expand_with(&handler, arena, &words, src);

    arena_alloc_free(&_arena);

    if (!rc) {
        dst->items = PF_ARRAY_GET(&words, 0);
        dst->length = PF_ARRAY_LEN(&words);
        dst->capacity = PF_ARRAY_CAP(&words);
        dst->allocator = &standard_allocator;
    } else {
        words_free(&words);
    }

    return PSTRING_OK;
}