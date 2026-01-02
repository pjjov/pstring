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

#include <pstring/pattern.h>
#include <pstring/pstring.h>

#include <stdalign.h>
#include <stdint.h>
#include <string.h>

#include "allocator_std.h"

#define BRANCH_SIZE (sizeof(size_t) + 1)
#define BRANCH_DEPTH 32
#define PARSER_DEPTH 64

enum {
    VAL_BYTE,
    VAL_CODE,
    VAL_CLASS,
};

struct value {
    int type;
    union {
        char byte;
        uint32_t code;
    } as;
};

#define VAL(type, ...)                         \
    (struct value) { VAL_##type, __VA_ARGS__ }

typedef struct pstrexpr_t {
    allocator_t *allocator;
    pstring_t source;
    pstring_t bytecode;
    pstring_t values;
} pstrexpr_t;

enum {
    OP_NOP = 0,
    OP_MATCH,
    OP_BRANCH,
    OP_CAPTURE_START,
    OP_CAPTURE_END,
};

struct parser {
    int errno;
    const char *chr;
    const char *end;

    const pstring_t *source;
    pstring_t *bytecode;
    pstring_t *vbuffer;

    size_t numCaptures;
    size_t numValues;
    struct value *values;
    size_t top;

    struct {
        int op;
        size_t index;
    } stack[PARSER_DEPTH];
};

static inline void emit_op(struct parser *p, char op) {
    if (pstrcatc(p->bytecode, op))
        p->errno = PSTRING_ENOMEM;
}

static inline void emit_buffer(struct parser *p, void *buffer, size_t length) {
    if (pstrcats(p->bytecode, (char *)buffer, length))
        p->errno = PSTRING_ENOMEM;
}

static inline void emit_value(struct parser *p, struct value *value) {
    if (pstrcats(p->vbuffer, (char *)value, sizeof(*value)))
        p->errno = PSTRING_ENOMEM;

    p->values = (struct value *)pstrbuf(p->vbuffer);
    p->numValues = pstrlen(p->vbuffer) * sizeof(*value);

    if (((uintptr_t)p->values) & (alignof(*value) - 1))
        p->errno = PSTRING_ENOMEM;
}

static inline void emit_index(struct parser *p, size_t index) {
    if (index < UINT8_MAX - 1) {
        emit_op(p, (uint8_t)index);
    } else {
        emit_op(p, UINT8_MAX);
        emit_buffer(p, &index, sizeof(index));
    }
}

static inline void emit_match(struct parser *p, size_t min, size_t max) {
    emit_op(p, OP_MATCH);
    emit_index(p, p->numValues - 1);
    emit_index(p, min);
    emit_index(p, max);
}

static inline void push_stack(struct parser *p, int op) {
    if (p->top < PARSER_DEPTH) {
        p->stack[p->top].op = op;
        p->stack[p->top].index = pstrlen(p->bytecode);
        p->top++;
    } else {
        p->errno = PSTRING_ENOMEM;
    }
}

static inline int pop_stack(struct parser *p, size_t *index) {
    if (p->top == 0)
        return 0;
    p->top--;

    if (index)
        *index = p->stack[p->top].index;
    return p->stack[p->top].op;
}

static inline int peek_stack(struct parser *p) {
    return p->top > 0 ? p->stack[p->top - 1].op : OP_NOP;
}

static inline void patch_branch(struct parser *p) {
    if (pstrreserve(p->bytecode, BRANCH_SIZE)) {
        p->errno = PSTRING_ENOMEM;
        return;
    }

    size_t branch;
    pop_stack(p, &branch);

    size_t jump = pstrlen(p->bytecode) - branch;
    char *buf = pstrslot(p->bytecode, branch);

    if (jump < UINT8_MAX - 1) {
        *buf = (uint8_t)jump;
        return;
    }

    *buf = UINT8_MAX;
    memcpy(buf + 1, buf + BRANCH_SIZE, pstrlen(p->bytecode) - branch - 1);
    memcpy(buf + 1, &jump, sizeof(jump));
}

static inline char peek(struct parser *p, size_t i) {
    return &p->chr[i] < p->end ? p->chr[i] : '\0';
};

static void regex_quantifier(struct parser *p) {
    /* todo: */
    size_t min = 0;
    size_t max = 0;
    emit_match(p, min, max);
}

static void regex_postfix(struct parser *p, size_t value) {
    char chr = peek(p, 0);

    switch (chr) {
        /* clang-format off */
    case '{': regex_quantifier(p);        break;
    case '?': emit_match(p, 0, 1);        break;
    case '*': emit_match(p, 0, SIZE_MAX); break;
    case '+': emit_match(p, 1, SIZE_MAX); break;
    default:  emit_match(p, 1, 1);        break;
        /* clang-format on */
    };

    emit_index(p, value);
}

static void regex_char(struct parser *p) {
    emit_value(p, &VAL(BYTE, .as.byte = peek(p, 0)));
    regex_postfix(p, p->numValues);
}

static void regex_any(struct parser *p) { regex_postfix(p, p->numValues); }

static inline int is_escape(char chr) {
    return NULL != strchr("{}[]()^$.|*+?\\", chr);
}

static inline int is_metaescape(char chr) {
    return NULL != strchr("dswDSW", chr);
}

static void regex_esc_wb(struct parser *p) { p->errno = PSTRING_ENOSYS; }

static void regex_esc_meta(struct parser *p) {
    emit_value(p, &VAL(CLASS, .as.byte = peek(p, 0)));
}

static void regex_esc(struct parser *p) {
    char chr = peek(p, 1);
    p->chr++;

    switch (chr) {
        /* clang-format off */
    case 't': chr = '\t'; break;
    case 'n': chr = '\n'; break;
    case 'r': chr = '\r'; break;
    /* todo */
        /* clang-format on */

    case 'b':
    case 'B':
        regex_esc_wb(p);
        break;

    default:
        if (is_escape(chr))
            regex_char(p);
        else if (is_metaescape(chr))
            regex_esc_meta(p);
        else
            p->errno = PSTRING_ENOENT;
    }
}

static void regex_set(struct parser *p) { }

static void regex_alt(struct parser *p) {
    patch_branch(p); /* previous */

    emit_op(p, OP_BRANCH);
    push_stack(p, OP_BRANCH);
    emit_index(p, 0);
}

static void regex_group_open(struct parser *p) {
    p->numCaptures++;
    emit_op(p, OP_CAPTURE_START);
    emit_op(p, OP_BRANCH); /* first alternation */
    push_stack(p, OP_CAPTURE_START);
    emit_index(p, 0);
}

static void regex_group_close(struct parser *p) {
    if (peek_stack(p) == OP_BRANCH)
        patch_branch(p);
    emit_op(p, OP_CAPTURE_END);
}

static void regex_next(struct parser *p) {
    char chr = peek(p, 0);
    p->chr++;

    if (chr & 0x80) {
        /* UTF-8 */
        p->errno = PSTRING_ENOSYS;
        return;
    }

    switch (chr) {
    case '*':
    case '?':
    case '+':
    case ']':
        p->errno = PSTRING_EINVAL;
        break;

        /* clang-format off */
    case '(':  regex_group_open(p);  break;
    case ')':  regex_group_close(p); break;
    case '\\': regex_esc(p);         break;
    case '[':  regex_set(p);         break;
    case '|':  regex_alt(p);         break;
    case '.':  regex_any(p);         break;
    default:   regex_char(p);        break;
        /* clang-format on */
    }
}

static void init_parser(pstrexpr_t *out, struct parser *p) {
    p->chr = pstrbuf(&out->source);
    p->end = pstrend(&out->source);

    p->errno = PSTRING_OK;
    p->numCaptures = 0;
    p->numValues = 0;
    p->top = 0;

    p->source = &out->source;
    p->vbuffer = &out->values;
    p->bytecode = &out->bytecode;
}

static int parse_regex(pstrexpr_t *out) {
    struct parser p;
    init_parser(out, &p);

    while (p.chr < p.end)
        regex_next(&p);

    return p.errno <= 0 ? p.errno : PSTRING_OK;
}

static int parse_pattern(pstrexpr_t *out) { return parse_regex(out); }

pstrexpr_t *pstrexpr_new(const char *pattern, allocator_t *allocator) {
    if (!allocator)
        allocator = &standard_allocator;

    pstrexpr_t *expr;

    if (!(expr = zallocate(allocator, sizeof(*expr))))
        return NULL;

    expr->allocator = allocator;
    int res = pstrnew(&expr->source, pattern, 0, allocator)
        || pstralloc(&expr->values, pstrcap(&expr->source), allocator)
        || pstralloc(&expr->bytecode, pstrcap(&expr->source), allocator)
        || parse_pattern(expr);

    if (res) {
        pstrexpr_free(expr);
        return NULL;
    }

    return expr;
}

void pstrexpr_free(pstrexpr_t *expr) {
    if (!expr)
        return;

    pstrfree(&expr->source);
    pstrfree(&expr->bytecode);
    pstrfree(&expr->values);
    deallocate(expr->allocator, expr, sizeof(*expr));
}
