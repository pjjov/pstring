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

#include <pstring/core.h>
#include <pstring/eval.h>
#include <pstring/format.h>
#include <pstring/glob.h>
#include <pstring/search.h>
#include <pstring/transform.h>
#include <pstring/wordexp.h>

#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allocator.h>
#include <allocator_arena.h>
#include <allocator_std.h>
#include <pf_ctype.h>
#include <pf_filesystem.h>
#include <pf_process.h>

#define PF_ARRAY_USE_ALLOCATOR_T
#define PF_ARRAY_DEFAULT_ALLOCATOR NULL
#include <pf_array.h>

#define MAX_ENV_NAME_LEN 256

/** A private sentinel byte, never meaningful in normal shell-word text,
    used to bracket text that came from inside a quote. `expand_string`
    (via `expand_single_quote`/`expand_double_quote`) writes a pair of
    these around whatever it copies out of a quoted span -- including
    the empty span, for `''`/`""` -- instead of just dropping the quote
    characters as it used to. `field_split` treats a byte pair as
    "quoting is on/off here" rather than as literal text, so IFS
    whitespace between the marks is no longer mistaken for a field
    separator, and strips the marks back out once a field's boundaries
    have been decided. **/
#define PSTR_QUOTE_MARK '\x01'

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

static int expand_command_tick(expand_state_t *state);
static int expand_command_paren(expand_state_t *state);
static int expand_braced(expand_state_t *state);
static int expand_arithmetic(expand_state_t *state);
static int expand_named(expand_state_t *state);
static int expand_pid(expand_state_t *state);
static int expand_status(expand_state_t *state);

static int is_invalid_char(char c) {
    return c == '|' || c == '&' || c == ';' || c == '<' || c == '>'
        || c == '\n';
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

    if (quote != '\0')
        return PSTRING_EINVAL;

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
    pstrrshift(state->src, 1); /* skip '~' */

    pstring_t username;
    pstring_t *src = state->src;
    const char *end = pstrpbrk(src, "/:");
    pstrrange(&username, NULL, pstrbuf(src), end ? end : pstrend(src));

    return call_handler(
        state->handler, PSTREXPAND_TILDE, &username, state->dst
    );
}

static int expand_single_quote(pstring_t *dst, pstring_t *src) {
    if (pstrget(src, 0) == '\'') {
        pstrrshift(src, 1); /* empty '' -- still a (zero-length) quoted
                                word, so mark it rather than writing
                                nothing at all */
        int rc = pstrcatc(dst, PSTR_QUOTE_MARK);
        return rc || pstrcatc(dst, PSTR_QUOTE_MARK);
    }

    const char *start = pstrbuf(src);
    const char *end = pstrend(src);

    const char *quote = pstrchr(src, '\'');

    int rc = pstrcatc(dst, PSTR_QUOTE_MARK);
    if (!rc && quote > start)
        rc = pstrcats(dst, start, quote - start);
    if (!rc)
        rc = pstrcatc(dst, PSTR_QUOTE_MARK);

    pstrrange(src, NULL, quote ? quote + 1 : end, end);
    return rc;
}

/** Decodes the body of a double-quoted string starting right after the
    opening `"` (already consumed by the caller). Per POSIX, inside
    double quotes:
    - `$` and `` ` `` keep their special meaning (parameter/command/
      arithmetic expansion still happens),
    - `'` is an ordinary character,
    - `\\` only escapes `$`, `` ` ``, `"`, `\\` and a following newline
      (which it deletes, i.e. a line continuation); any other `\\x` is
      copied through literally, backslash included. **/
static int expand_double_quote(expand_state_t *state) {
    pstring_t *dst = state->dst;
    pstring_t *src = state->src;

    if (pstrget(src, 0) == '"') {
        pstrrshift(src, 1); /* empty "" -- same reasoning as '' above */
        int rc = pstrcatc(dst, PSTR_QUOTE_MARK);
        return rc || pstrcatc(dst, PSTR_QUOTE_MARK);
    }

    int res = pstrcatc(dst, PSTR_QUOTE_MARK);
    if (res)
        return res;

    const char *end = pstrend(src);
    const char *prev = pstrbuf(src);
    const char *special;

    while (!res && (special = pstrpbrk(src, "$`\"\\"))) {
        if ((res = pstrcatb(dst, prev, special - prev)))
            break;

        if (*special == '"') {
            pstrrange(src, NULL, special + 1, end);
            return pstrcatc(dst, PSTR_QUOTE_MARK);
        }

        pstrrange(src, NULL, special + 1, end);

        switch (*special) {
        case '\\': {
            char c = pstrget(src, 0);
            if (c == '$' || c == '`' || c == '"' || c == '\\') {
                res = pstrcatc(dst, c);
                pstrrshift(src, 1);
            } else if (c == '\n') {
                pstrrshift(src, 1); /* line continuation: drop both chars */
            } else {
                res = pstrcatc(dst, '\\'); /* not a recognised escape */
            }
            break;
        }
        case '`':
            res = expand_command_tick(state);
            break;
        case '$': {
            char curr = pstrget(src, 0);
            char next = pstrget(src, 1);

            if (curr == '(' && next == '(')
                res = expand_arithmetic(state);
            else if (curr == '(')
                res = expand_command_paren(state);
            else if (curr == '{')
                res = expand_braced(state);
            else if (pf_isalpha(curr) || curr == '_')
                res = expand_named(state);
            else if (curr == '$')
                res = expand_pid(state);
            else if (curr == '?')
                res = expand_status(state);
            else
                res = pstrcatc(dst, '$');
            break;
        }
        }

        prev = pstrbuf(src);
    }

    /* ran off the end without seeing the closing '"' -- `check_invalid_chars`
       is supposed to catch this earlier, so reaching it here means the two
       scans disagree about what is/isn't inside a quote */
    return res ? res : PSTRTHROW_EINVAL;
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
    const char *end = pstrend(state->src);
    const char *close = find_closing_quote(state->src, '`');

    if (!close)
        return PSTRING_EINVAL;

    pstring_t cmd;
    pstrrange(&cmd, NULL, start, close);

    pstrrange(state->src, NULL, close + 1, end);

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
            res = expand_double_quote(state);
            break;
        case '\'':
            res = expand_single_quote(dst, src);
            break;
        case '`':
            res = expand_command_tick(state);
            break;
        case '$': {
            char curr = pstrget(src, 0);
            char next = pstrget(src, 1);

            if (curr == '(' && next == '(')
                res = expand_arithmetic(state);
            else if (curr == '(')
                res = expand_command_paren(state);
            else if (curr == '{')
                res = expand_braced(state);
            else if (pf_isalpha(curr) || curr == '_')
                res = expand_named(state);
            else if (curr == '$')
                res = expand_pid(state);
            else if (curr == '?')
                res = expand_status(state);
            else
                res = pstrcatc(dst, '$');
            break;
        }
        case '~':
            if (special == start || special[-1] == ':' || special[-1] == '=') {
                res = expand_tilde(state);
            } else {
                pstrcatc(dst, '~');
                pstrrshift(src, 1);
            }
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

    if (pstrlen(src) == 0)
        return PSTRING_OK;

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

/** True if `c` is one of the characters in `state->ifs` (the whole set,
    whitespace or not). **/
static int is_ifs_char(split_state_t *state, char c) {
    for (const char *p = pstrbuf(&state->ifs); *p; p++)
        if (*p == c)
            return 1;
    return 0;
}

/** Scans [pstrbuf(src), pstrend(src)) for the first IFS character that
    is outside a quoted span (a `PSTR_QUOTE_MARK`-delimited run left by
    `expand_string`), toggling quote state on each mark byte without
    ever treating a mark itself as a delimiter. Returns a pointer to
    that character, or NULL if the rest of `src` has no such
    delimiter -- meaning whatever quoted whitespace it contains, if
    any, is protected and belongs to a single field. **/
static const char *find_delim(split_state_t *state, pstring_t *src) {
    int inQuote = 0;

    for (const char *p = pstrbuf(src); p < pstrend(src); p++) {
        if (*p == PSTR_QUOTE_MARK) {
            inQuote = !inQuote;
            continue;
        }
        if (!inQuote && is_ifs_char(state, *p))
            return p;
    }

    return NULL;
}

/** Copies [start,end) to `dst`, dropping every `PSTR_QUOTE_MARK` byte
    -- the marks have already done their job of protecting this field's
    text from being split on, and must not leak into the final word. **/
static int strip_marks(pstring_t *dst, const char *start, const char *end) {
    int rc = PSTRING_OK;

    for (const char *p = start; !rc && p < end; p++)
        if (*p != PSTR_QUOTE_MARK)
            rc = pstrcatc(dst, *p);

    return rc;
}

static int field_split(split_state_t *state) {
    if (init_ifs_tables(state)) { /* empty IFS -- the whole (expanded)
                                      string is a single field, but it
                                      may still carry quote marks that
                                      need to come out first */
        pstring_t clean = { 0 };
        int rc = strip_marks(&clean, pstrbuf(state->src), pstrend(state->src));
        if (!rc)
            rc = words_push_dup(state->words, &clean);
        pstrfree(&clean);
        return rc;
    }

    pstrlstrip(state->src, state->ws);

    pstring_t *src = state->src;
    const char *ws = pstrbuf(src);
    int rc = PSTRING_OK;

    while (!rc && ws < pstrend(src)) {
        if (!(ws = find_delim(state, src)))
            ws = pstrend(src);

        pstring_t field;
        pstrrange(&field, NULL, pstrbuf(src), ws);
        pstrrange(src, NULL, ws, pstrend(src));

        pstrrstrip(&field, state->ws);

        pstring_t clean = { 0 };
        rc = strip_marks(&clean, pstrbuf(&field), pstrend(&field));
        if (!rc)
            rc = words_push_dup(state->words, &clean);
        pstrfree(&clean);

        if (!rc)
            rc = fs_skip_ws(state);
    }

    return rc;
}

static int expand_pathname(path_state_t *state) {
    for (size_t i = 0; i < PF_ARRAY_LEN(state->words); i++) {
        pstring_t *word = PF_ARRAY_SLOT(state->words, i);

        int hasGlob = pstrpbrk(word, "*?[") != NULL;

        if (hasGlob) {
            int rc = call_handler(
                state->handler, PSTREXPAND_GLOB, word, state->result
            );

            if (rc != PSTRING_OK)
                return rc;
        } else {
            pstring_t clean;
            if (pstrdup(&clean, word, NULL))
                return PSTRING_ENOMEM;
            PF_ARRAY_PUSH(state->result, &clean, 1);
        }
    }

    return PSTRING_OK;
}

static void words_free(words_t *words) {
    for (size_t i = 0; i < PF_ARRAY_LEN(words); i++)
        pstrfree(PF_ARRAY_SLOT(words, i));
    PF_ARRAY_FREE(words);
}

/** A run of `src` that is either entirely outside quotes (`quoted ==
    0`) or a single `'...'`/`"..."` pair including its delimiters
    (`quoted == 1`). Used by `brace_expand_word` to keep brace
    expansion from reaching into quoted text, matching the shell rule
    that `'{a,b}'` is not a brace expression. **/
typedef struct {
    const char *start;
    const char *end;
    int quoted;
} word_span_t;

typedef PF_ARRAY(word_span_t) word_spans_t;

/** Splits `src` into alternating quoted/unquoted spans for
    `brace_expand_word`. This only needs to track *which* regions are
    quoted, not decode their contents (that's `expand_string`'s job,
    which still runs on each brace alternative afterwards) -- so it
    only has to agree with `check_invalid_chars` about where quotes
    start and end, including the same backslash-escaping rules. **/
static int split_quote_spans(pstring_t *src, word_spans_t *spans) {
    const char *end = pstrend(src);
    const char *segStart = pstrbuf(src);
    const char *p = segStart;

    while (p < end) {
        char c = *p;

        if (c == '\\' && p + 1 < end) {
            p += 2;
            continue;
        }

        if (c == '\'' || c == '"') {
            if (p > segStart) {
                word_span_t unquoted = { segStart, p, 0 };
                if (PF_ARRAY_PUSH(spans, &unquoted, 1))
                    return PSTRING_ENOMEM;
            }

            const char *qstart = p++;

            if (c == '\'') {
                while (p < end && *p != '\'')
                    p++;
            } else {
                while (p < end) {
                    if (*p == '\\' && p + 1 < end) {
                        p += 2;
                        continue;
                    }
                    if (*p == '"')
                        break;
                    p++;
                }
            }

            if (p < end)
                p++; /* consume the closing quote */

            word_span_t quoted = { qstart, p, 1 };
            if (PF_ARRAY_PUSH(spans, &quoted, 1))
                return PSTRING_ENOMEM;

            segStart = p;
            continue;
        }

        p++;
    }

    if (p > segStart) {
        word_span_t unquoted = { segStart, p, 0 };
        if (PF_ARRAY_PUSH(spans, &unquoted, 1))
            return PSTRING_ENOMEM;
    }

    return PSTRING_OK;
}

/** Brace-expands `src` into one or more raw (still-quoted,
    still-otherwise-unexpanded) alternatives in `out`, which
    `expand_with` then runs through the normal
    expand_string/field_split/expand_pathname pipeline once per
    alternative. Quoted spans (see `split_quote_spans`) are kept as a
    single, unexpanded alternative each -- `'{a,b}'` is not a brace
    expression -- while every unquoted span is expanded with the
    public `pstrbrace`; the whole word's alternatives are the cross
    product of every span's alternatives, in order. **/
static int brace_expand_word(words_t *out, pstring_t *src, allocator_t *arena) {
    word_spans_t spans;
    PF_ARRAY_WITH_ALLOCATOR(&spans, arena);

    int rc = split_quote_spans(src, &spans);
    if (rc) {
        PF_ARRAY_FREE(&spans);
        return rc;
    }

    words_t acc;
    PF_ARRAY_WITH_ALLOCATOR(&acc, arena);

    pstring_t empty;
    pstrwrap(&empty, "", 0, 0);
    if ((rc = words_push_dup(&acc, &empty))) {
        PF_ARRAY_FREE(&spans);
        words_free(&acc);
        return rc;
    }

    for (size_t i = 0; !rc && i < PF_ARRAY_LEN(&spans); i++) {
        word_span_t *sp = PF_ARRAY_SLOT(&spans, i);
        pstring_t spanText;
        pstrrange(&spanText, src, sp->start, sp->end);

        words_t spanAlts;
        PF_ARRAY_WITH_ALLOCATOR(&spanAlts, arena);

        if (sp->quoted) {
            rc = words_push_dup(&spanAlts, &spanText);
        } else {
            pstrarray_t braced = { 0 };
            rc = pstrbrace(&braced, &spanText, NULL);
            for (size_t k = 0; !rc && k < braced.length; k++)
                rc = words_push_dup(&spanAlts, &braced.items[k]);
            pstrarray_free(&braced);
        }

        if (rc) {
            words_free(&spanAlts);
            break;
        }

        words_t next;
        PF_ARRAY_WITH_ALLOCATOR(&next, arena);

        for (size_t a = 0; !rc && a < PF_ARRAY_LEN(&acc); a++) {
            for (size_t b = 0; !rc && b < PF_ARRAY_LEN(&spanAlts); b++) {
                pstring_t combined;
                rc = pstrdup(&combined, PF_ARRAY_SLOT(&acc, a), NULL);
                if (!rc)
                    rc = pstrcat(&combined, PF_ARRAY_SLOT(&spanAlts, b));
                if (!rc)
                    rc = PF_ARRAY_PUSH(&next, &combined, 1) ? PSTRING_ENOMEM
                                                            : PSTRING_OK;
                if (rc)
                    pstrfree(&combined);
            }
        }

        words_free(&acc);
        words_free(&spanAlts);
        acc = next;
    }

    PF_ARRAY_FREE(&spans); /* word_span_t elements aren't owned */

    if (rc) {
        words_free(&acc);
        return rc;
    }

    *out = acc;
    return PSTRING_OK;
}

/** Runs one already brace-resolved word through expand_string (`$`,
    backtick, arithmetic, tilde, quote removal), then field_split, then
    expand_pathname, appending the results to `result`. This is what
    `expand_with` used to do directly, before brace expansion was
    inserted as a preprocessing step that can turn one raw word into
    several of these. **/
static int expand_one_word(
    pstrexpand_t *handler,
    allocator_t *arena,
    words_t *result,
    pstring_t *rawWord
) {
    pstring_t expanded = { 0 };
    if (pstralloc(&expanded, 4096, arena))
        return PSTRING_ENOMEM;

    pstring_t srcSlice;
    pstrslice(&srcSlice, rawWord, 0, pstrlen(rawWord));

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

    return expand_pathname(&pathState);
}

/** Brace-expands `src` (see `brace_expand_word`) and then runs each
    resulting alternative through `expand_one_word`, appending all of
    their results to `result` in order. This is the top-level per-word
    expansion entry point `pstrexpand_with` calls. **/
static int expand_with(
    pstrexpand_t *handler, allocator_t *arena, words_t *result, pstring_t *src
) {
    words_t alts;
    int rc = brace_expand_word(&alts, src, arena);
    if (rc)
        return rc;

    for (size_t i = 0; !rc && i < PF_ARRAY_LEN(&alts); i++)
        rc = expand_one_word(handler, arena, result, PF_ARRAY_SLOT(&alts, i));

    words_free(&alts);
    return rc;
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

    arena_allocator_t _arena = { 0 };
    arena_allocator_init(&_arena, &standard_allocator);
    _arena.alignment = 32;
    allocator_t *arena = &_arena.alloc;

    words_t words;
    PF_ARRAY_WITH_ALLOCATOR(&words, &standard_allocator);

    int rc = expand_with(&handler, arena, &words, src);

    arena_allocator_free(&_arena);

    if (!rc) {
        dst->items = PF_ARRAY_GET(&words, 0);
        dst->length = PF_ARRAY_LEN(&words);
        dst->capacity = PF_ARRAY_CAP(&words);
        dst->allocator = &standard_allocator;
    } else {
        words_free(&words);
    }

    return rc;
}

static int default_expand_tilde(
    pstring_t *out, const pstring_t *username, int flags
) {
    (void)flags;

    /* pf_homedir requires a NUL-terminated string. */
    char name[256];
    const char *cname = pstrterms((pstring_t *)username, name, sizeof(name));
    if (!cname)
        return PSTRING_ENOMEM;

    char path[4096];
    if (!pf_homedir(name, path, sizeof(path)))
        return PSTRING_ENOENT;

    return pstrcats(out, path, 0);
}

static int default_expand_pid(pstring_t *out) {
    return pstrfmt(out, "%ld", (long)getpid());
}

/** Runs `cmd` (already-expanded shell text) through the platform shell
    and appends its standard output to `out`, with the trailing run of
    newlines stripped -- matching POSIX command substitution, which
    always removes trailing newlines (but nothing else) from the
    captured output. **/
static int default_expand_command(pstring_t *out, const pstring_t *cmd) {
    /* `cmd` is a slice into the word currently being expanded, not an
       owned string -- `pstrunwrap` hands back ownership of an existing
       heap buffer (or duplicates an SSO one), neither of which applies
       to a plain slice, so the pointer it returned here was not one
       `free()` could safely take. Make an explicit owned copy (which is
       always NUL-terminated) and free it the pstring way instead. */
    pstring_t owned = { 0 };
    if (pstrdup(&owned, cmd, NULL))
        return PSTRING_ENOMEM;

    FILE *pipe = popen(pstrbuf(&owned), "r");
    pstrfree(&owned);

    if (!pipe)
        return PSTREXPAND_CMDSUB;

    char chunk[4096];
    size_t n;
    int rc = PSTRING_OK;

    while (!rc && (n = fread(chunk, 1, sizeof(chunk), pipe)) > 0)
        rc = pstrcats(out, chunk, n);

    pclose(pipe);

    if (rc)
        return rc;

    /* strip only the trailing newlines, not trailing whitespace in
       general -- a command that prints "a \n" keeps the trailing space */
    size_t len = pstrlen(out);
    while (len > 0 && pstrget(out, len - 1) == '\n')
        len--;
    pstr__setlen(out, len);

    return PSTRING_OK;
}

/** Evaluates `expr` (the text between `$((` and `))`) via `pstreval`,
    resolving bare identifiers as shell/environment variables. Nested
    expansions ($VAR inside the arithmetic expression, which POSIX also
    allows) are intentionally not re-run here: `pstreval`'s own
    identifier resolution already covers the common case of referencing
    a variable by name, which is what shell arithmetic almost always
    does in practice. **/
static int arith_get(long long *out, const pstring_t *name, void *user) {
    (void)user;
    pstring_t value;
    int rc = pstrenv(&value, name);
    if (rc == PSTRING_ENOENT) {
        *out = 0;
        return PSTRING_OK;
    }
    if (rc)
        return rc;

    char buf[64];
    const char *cstr = pstrterms(&value, buf, sizeof(buf));
    if (!cstr)
        return PSTRING_ENOMEM;

    char *stop;
    *out = strtoll(cstr, &stop, 0);
    return *stop == '\0' ? PSTRING_OK : PSTREXPAND_BADVAL;
}

static int default_expand_arithmetic(pstring_t *out, pstring_t *expr) {
    long long value;
    int rc = pstreval(&value, expr, arith_get, NULL, NULL);
    if (rc)
        return rc == PSTRING_EINVAL ? PSTREXPAND_SYNTAX : rc;
    return pstrfmt(out, "%lld", value);
}

/** The actual pattern-removal logic behind `${var#pattern}` and its
    three siblings, and the default implementation of the
    `PSTREXPAND_TRIM` hook. `pstrglob_match` is a whole-string matcher,
    so "does the pattern match a prefix/suffix of length n" is checked
    by matching it against just that slice of `value`; trying every
    length from empty up to the whole string (or the other way around
    for the greedy `##`/`%%` forms) finds the shortest/longest matching
    prefix or suffix directly, with no need for a dedicated
    prefix-of-a-glob matcher. If nothing matches at all -- including
    the empty slice, which is what makes `${var#*}` a no-op, since `*`
    matches zero characters too -- `value` is returned unchanged, same
    as a real shell. **/
static int default_expand_trim(pstring_t *out, const pstrexpand_trim_t *req) {
    char patBuf[256];
    const char *pattern = pstrterms(req->pattern, patBuf, sizeof(patBuf));
    if (!pattern)
        return PSTRING_ENOMEM;

    const pstring_t *value = req->value;
    long len = (long)pstrlen(value);
    long best = -1;

    long start = req->greedy ? len : 0;
    long stop = req->greedy ? -1 : len + 1;
    long step = req->greedy ? -1 : 1;

    for (long n = start; n != stop; n += step) {
        pstring_t piece;
        if (req->suffix)
            pstrrange(&piece, value, pstrend(value) - n, pstrend(value));
        else
            pstrrange(&piece, value, pstrbuf(value), pstrbuf(value) + n);

        int rc = pstrglob_match(pattern, &piece, 0);
        if (rc < 0)
            return rc;
        if (rc == PSTRING_TRUE) {
            best = n;
            break;
        }
    }

    pstring_t remaining;
    if (best < 0) {
        remaining = *value;
    } else if (req->suffix) {
        pstrrange(&remaining, value, pstrbuf(value), pstrend(value) - best);
    } else {
        pstrrange(&remaining, value, pstrbuf(value) + best, pstrend(value));
    }

    return pstrcat(out, &remaining);
}

/** Implements the common POSIX `${...}` parameter-expansion operators:
    `${var}` (plain), `${var:-word}` (use default), `${var:=word}`
    (assign default -- accepted syntactically, but since pstring has no
    variable *store* to write back to, this behaves like `:-`),
    `${var:+word}` (use alternate value), `${var:?word}` (error if
    unset), `${#var}` (length), and the pattern-removal family
    `${var#pattern}`/`${var##pattern}` (strip the shortest/longest
    matching prefix) and `${var%pattern}`/`${var%%pattern}` (strip the
    shortest/longest matching suffix). Nested `$`/backtick expansion
    inside `word`/`pattern` is deliberately not attempted. **/
static int default_expand_brace(pstring_t *out, pstring_t *body, int flags) {
    int wantLength = pstrget(body, 0) == '#';
    if (wantLength)
        pstrrshift(body, 1);

    const char *nameEnd = pstrcpbrk(body, PF_CTYPE_ALNUM "_");
    pstring_t name;
    pstrrange(&name, NULL, pstrbuf(body), nameEnd ? nameEnd : pstrend(body));

    pstring_t value;
    int rc = pstrenv(&value, &name);
    int isSet = (rc == PSTRING_OK);

    if (wantLength) {
        if (!isSet) {
            if (flags & PSTREXPAND_UNDEF)
                return PSTREXPAND_BADVAL;
            return pstrcatc(out, '0');
        }

        return pstrfmt(out, "%zu", pstrlen(&value));
    }

    if (!nameEnd || nameEnd == pstrend(body)) {
        /* bare ${var}, nothing following the name */
        if (isSet)
            return pstrcat(out, &value);
        return (flags & PSTREXPAND_UNDEF) ? PSTREXPAND_BADVAL : PSTRING_OK;
    }

    char op = *nameEnd;

    if (op == '#' || op == '%') {
        if (!isSet)
            return (flags & PSTREXPAND_UNDEF) ? PSTREXPAND_BADVAL : PSTRING_OK;

        pstring_t pattern;
        pstrrange(&pattern, NULL, nameEnd + 1, pstrend(body));

        int greedy = pstrget(&pattern, 0) == op;
        if (greedy)
            pstrrshift(&pattern, 1);

        pstrexpand_trim_t req = {
            .value = &value,
            .pattern = &pattern,
            .suffix = (op == '%'),
            .greedy = greedy,
        };
        return default_expand_trim(out, &req);
    }

    if (op != ':') {
        /* unrecognised ${var<op>...} */
        return PSTRING_ENOSYS;
    }

    pstring_t rest;
    pstrrange(&rest, NULL, nameEnd + 1, pstrend(body));
    char sub = pstrget(&rest, 0);
    pstrrshift(&rest, 1);

    switch (sub) {
    case '-': /* use default if unset or empty */
        if (isSet && pstrlen(&value) > 0)
            return pstrcat(out, &value);
        return pstrcat(out, &rest);
    case '=': /* assign default -- no variable store, so behaves as ':-' */
        if (isSet && pstrlen(&value) > 0)
            return pstrcat(out, &value);
        return pstrcat(out, &rest);
    case '+': /* use alternate value if set and non-empty */
        if (isSet && pstrlen(&value) > 0)
            return pstrcat(out, &rest);
        return PSTRING_OK;
    case '?': /* error with message if unset or empty */
        if (isSet && pstrlen(&value) > 0)
            return pstrcat(out, &value);
        return PSTREXPAND_BADVAL;
    default:
        return PSTRING_ENOSYS;
    }
}

static int default_expand_named(pstring_t *out, pstring_t *name, int flags) {
    pstring_t value;
    int rc = pstrenv(&value, name);

    if (rc == PSTRING_ENOENT)
        return (flags & PSTREXPAND_UNDEF) ? PSTREXPAND_BADVAL : PSTRING_OK;

    if (rc == PSTRING_OK)
        return pstrcat(out, &value);
    return rc;
}

static int default_expand_glob(words_t *result, pstring_t *pattern) {
    pstrarray_t matches = { 0 };
    int rc = pstrglob(&matches, pattern, PSTRGLOB_NOCHECK, NULL);
    if (rc)
        return rc;

    for (size_t i = 0; i < matches.length; i++) {
        if (words_push_dup(result, &matches.items[i])) {
            pstrarray_free(&matches);
            return PSTRING_ENOMEM;
        }
    }
    pstrarray_free(&matches);
    return PSTRING_OK;
}

int pstrexpand_default_cb(
    void *dst, void *src, int flags, int kind, void *user
) {
    (void)user;

    switch (kind) {
    case PSTREXPAND_NONE:
        return PSTRING_OK;
    case PSTREXPAND_NAMED:
        return default_expand_named(dst, src, flags);
    case PSTREXPAND_TILDE:
        return default_expand_tilde(dst, src, flags);
    case PSTREXPAND_STATUS:
        return pstrcatc(dst, '0');
    case PSTREXPAND_PID:
        return default_expand_pid(dst);
    case PSTREXPAND_IFS:
        return pstrenv(dst, PSTR("IFS"));
    case PSTREXPAND_BRACE:
        return default_expand_brace(dst, src, flags);
    case PSTREXPAND_CMD_PAREN:
    case PSTREXPAND_CMD_TICK:
        if (flags & PSTREXPAND_NOCMD)
            return PSTREXPAND_CMDSUB;
        return default_expand_command(dst, src);
    case PSTREXPAND_ARITHMETIC:
        return default_expand_arithmetic(dst, src);
    case PSTREXPAND_GLOB:
        return default_expand_glob(dst, src);
    case PSTREXPAND_TRIM:
        return default_expand_trim(dst, src);
    }

    return PSTRTHROW_EINVAL;
}

int pstrexpand(pstrarray_t *dst, pstring_t *src, int flags, pstrexpand_fn *cb) {
    return pstrexpand_with(
        dst, src, flags, cb ? cb : pstrexpand_default_cb, NULL
    );
}
