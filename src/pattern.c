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

#include <pstring/pstring.h>
#include <stdint.h>

enum {
    VAL_BYTE,
    VAL_CODE,
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
    pstring_t bytecode;
    pstring_t values;
} pstrexpr_t;

enum {
    OP_NOP = 0,
    OP_MATCH,
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

    size_t vcount;
    struct value *values;
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
    p->vcount = pstrlen(p->vbuffer) * sizeof(*value);

    if (((uintptr_t)p->values) & (sizeof(*value) - 1))
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
    emit_index(p, p->vcount - 1);
    emit_index(p, min);
    emit_index(p, max);
}

static inline char peek(struct parser *p, size_t i) {
    return &p->chr[i] < p->end ? p->chr[i] : '\0';
};
