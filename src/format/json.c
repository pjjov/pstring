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

#include <pstring/dictionary.h>
#include <pstring/encoding.h>
#include <pstring/io.h>
#include <pstring/object.h>
#include <pstring/pstring.h>

#include <stdlib.h>
#include <string.h>

#define PF_TYPE_HELPERS
#include <pf_macro.h>
#include <pf_typeid.h>

#define JSON_STREAM(x) ((struct json_stream *)&(x)->state._size)
#define JSON_BUFFER_SIZE 4096

struct json_lexer {
    pstream_t *stream;
    size_t start;
    size_t end;
    int eof;
    char buf[JSON_BUFFER_SIZE];
};

struct json_reader {
    struct json_lexer lexer;
    pstream_t *stream;
    int prev, curr;
    unsigned int failed : 1;
    pstring_t prevValue, currValue;
};

struct json_writer {
    pstream_t *base;
    int prev;

    const struct pstrmodel_member *member;
};

static int json_serialize(
    struct json_writer *json,
    const struct pstrmodel_member *member,
    const void *item
);

static int json_write_int(pstream_t *stream, int type, const void *item) {
    char buffer[256];
    uintmax_t value;
    int res;

    if (pf_type_int_load(type, item, &value))
        return PSTRING_EINVAL;

    if (pf_type_is_unsigned(type))
        res = snprintf(buffer, 256, "%llu", (unsigned long long)value);
    else
        res = snprintf(buffer, 256, "%lld", (long long)value);

    if (res <= 0 || res >= 256)
        return PSTRING_EINVAL;

    if (res != pstream_write(stream, buffer, res))
        return PSTRING_EIO;

    return PSTRING_OK;
}

static int json_write_float(pstream_t *stream, int type, const void *item) {
    char buffer[256];
    double value;
    int res;

    switch (type) {
        /* clang-format off */
    case PF_TYPE_FLOAT:   value = *(float *)item; break;
    case PF_TYPE_DOUBLE:  value = *(double *)item; break;
    case PF_TYPE_LDOUBLE: value = *(long double *)item; break;
    default: return PSTRING_EINVAL;
        /* clang-format on */
    }

    res = snprintf(buffer, 256, "%f", value);
    if (res <= 0 || res >= 256)
        return PSTRING_EINVAL;

    if (res != pstream_write(stream, buffer, res))
        return PSTRING_EIO;

    return PSTRING_OK;
}

static int json_serialize_array(
    pstream_t *stream, const void *obj, struct pstrmodel_array *model
) {
    pstream_putc(stream, '[');
    int res = PSTRING_OK;

    for (size_t i = 0; !res && i < model->count; i++) {
        if (i > 0)
            pstream_putc(stream, ',');

        const void *item = PF_OFFSET(obj, model->stride * i);
        res = pstream_save_json(stream, item, model->submodel);
    }

    pstream_putc(stream, ']');
    return res;
}

static int json_serialize_llist(
    pstream_t *stream, const void *obj, struct pstrmodel_llist *model
) {
    pstream_putc(stream, '[');
    int res = PSTRING_OK;

    const void *head = *(const void **)obj;
    const void *item, **link;

    for (item = head; item; item = *link) {
        if (item != head)
            pstream_putc(stream, ',');

        link = PF_OFFSET(item, model->linkOffset);
        res = pstream_save_json(stream, item, model->submodel);
    }

    pstream_putc(stream, ']');
    return res;
}

struct json_serialize_dict_state {
    pstream_t *stream;
    struct pstrmodel_member *member;
};

static int json_serialize_dict_each(void *user, pstring_t *key, void *value) {
    struct json_writer *json = user;
    return pstream_printf(json->base, "\"%!json%P\":", key)
        || json_serialize(json, value, json->member);
}

static int json_serialize_dict(
    struct json_writer *json, const void *obj, struct pstrmodel_member *member
) {
    pstream_putc(json->base, '{');
    json->member = member;

    pstrdict_t *dict = *(pstrdict_t **)obj;
    int res = pstrdict_each(dict, json_serialize_dict_each, json);

    pstream_putc(json->base, '}');
    return res;
}

static int json_serialize(
    struct json_writer *json,
    const struct pstrmodel_member *member,
    const void *item
) {
    pstring_t str;
    int res = PSTRING_OK;

    switch (member->type) {
    case PF_TYPE_BOOL: {
        pf_bool value = *(pf_bool *)item;
        res = pstream_puts(json->base, value ? "true" : "false");
        break;
    }

    case PF_TYPE_CHAR:
        res = pstream__printf(json->base, "\"%c\"", *(char *)item);
        break; /* todo: escape character */
    case PF_TYPE_CSTRING:
        pstrwrap(&str, *(char **)item, 0, 0);
        res = pstream_printf(json->base, "\"%!json%P\"", &str);
        break;
    case PSTRING_TYPE:
        res = pstream_printf(json->base, "\"%!json%P\"", item);
        break;
    case PSTRING_PTR_TYPE:
        res = pstream_printf(json->base, "\"%!json%P\"", *(pstring_t **)item);
        break;

    case PF_TYPE_PTR:
        res = pstream__printf(json->base, "\"%p\"", *(void **)item);
        break;

    case PSTRMODEL_TYPE:
        return pstream_save_json(json->base, item, member->model);
    case PSTRMODEL_ARRAY:
        return json_serialize_array(json->base, item, member->model);
    case PSTRMODEL_LLIST:
        return json_serialize_llist(json->base, item, member->model);
    case PSTRDICT_TYPE:
        return json_serialize_dict(json, item, member->model);

    default:
        if (pf_type_is_integer(member->type))
            res = json_write_int(json->base, member->type, item);
        else if (pf_type_is_float(member->type))
            res = json_write_float(json->base, member->type, item);
        else
            res = PSTRING_ENOSYS;
    }

    json->prev = member->type;
    return res;
}

static int json_write_key(pstream_t *stream, const char *key) {
    pstring_t str;
    pstrwrap(&str, (char *)key, 0, 0);
    return pstream_printf(stream, "\"%!json%P\":", &str);
}

static int json_save_member(
    struct json_writer *json,
    const void *obj,
    const struct pstrmodel_member *member
) {
    const void *item = PF_OFFSET(obj, member->offset);

    if (json->prev != PSTRMODEL__BEGIN && pstream_putc(json->base, ','))
        return PSTRING_EIO;

    if (json_write_key(json->base, member->name))
        return PSTRING_EIO;

    return json_serialize(json, member, item);
}

int pstream_save_json(
    pstream_t *stream, const void *obj, const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRING_EINVAL;

    int result = PSTRING_OK;
    struct json_writer json;
    json.base = stream;
    json.prev = PSTRMODEL__BEGIN;

    result |= pstream_putc(json.base, '{');

    for (size_t i = 0; !result && model->members[i].type; i++)
        result |= json_save_member(&json, obj, &model->members[i]);

    result |= pstream_putc(json.base, '}');

    return result;
}

static int json_isblank(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static int json_reserve(struct json_lexer *lex, size_t count) {
    size_t diff = lex->end - lex->start;

    if (lex->eof || diff >= count)
        return PSTRING_OK;

    if (lex->start > 0) {
        if (diff > 0)
            memmove(lex->buf, &lex->buf[lex->start], diff);
        lex->start = 0;
        lex->end = diff;
    }

    size_t read = pstream_read(
        lex->stream, &lex->buf[lex->end], JSON_BUFFER_SIZE - diff
    );

    if (read == 0)
        lex->eof = PSTRING_TRUE;
    lex->end += read;

    return diff + read < count ? PSTRING_ENOMEM : PSTRING_OK;
}

static int json_skip_blank(struct json_lexer *lex) {
    while (!json_reserve(lex, 1)) {
        if (!json_isblank(lex->buf[lex->start]))
            break;
        lex->start++;
    }

    return lex->eof == 0 ? PSTRING_OK : PSTRING_EIO;
}

static int json_read_number(struct json_lexer *lex) {
    int len = 1;

    while (!json_reserve(lex, len + 1)) {
        char c = lex->buf[lex->start + len];

        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.'
              || c == 'e' || c == 'E'))
            return len;

        len++;
    }

    return lex->eof == 0 ? PSTRING_OK : PSTRING_EIO;
}

static int json_read_string(struct json_lexer *lex, pstring_t *out) {
    int len = 1;
    int escape = PSTRING_FALSE;
    int quote = PSTRING_FALSE;

    while (!json_reserve(lex, len)) {
        char c = lex->buf[lex->start + len];

        if (c == '"' && !escape) {
            quote = PSTRING_TRUE;
            break;
        }

        if (c == '\\' || escape)
            escape = !escape;

        len++;
    }

    if (quote) {
        pstrrange(
            out, NULL, &lex->buf[lex->start + 1], &lex->buf[lex->start + len]
        );

        lex->start += len + 1;
    }

    return lex->eof == 0 || quote ? PSTRING_OK : PSTRING_EIO;
}

static int json_read_keyword(
    struct json_lexer *lex, const char *kw, size_t len
) {
    if (json_reserve(lex, len))
        return PSTRING_EINVAL;

    if (memcmp(kw, &lex->buf[lex->start], len))
        return PSTRING_EINVAL;

    lex->start += len;
    return PSTRING_OK;
}

static int json_read_token(struct json_lexer *lex, pstring_t *out) {
    json_skip_blank(lex);

    if (json_reserve(lex, 1))
        return PSTRING_EIO;

    char c = lex->buf[lex->start];

    switch (c) {
    case '{':
    case '}':
    case '[':
    case ']':
    case ':':
    case ',':
        lex->start++;
        return c;
    case '"':
        if (json_read_string(lex, out))
            return PSTRING_EINVAL;
        return '"';
    case 't':
        if (json_read_keyword(lex, "true", 4))
            return PSTRING_EINVAL;
        return 't';
    case 'f':
        if (json_read_keyword(lex, "false", 5))
            return PSTRING_EINVAL;
        return 'f';
    case 'n':
        if (json_read_keyword(lex, "null", 4))
            return PSTRING_EINVAL;
        return 'n';
    default:
        if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
            int len = json_read_number(lex);

            pstrrange(
                out, NULL, &lex->buf[lex->start], &lex->buf[lex->start + len]
            );

            lex->start += len;
            return 'd';
        }

        return PSTRING_EINVAL;
    }
}

static void json_advance(struct json_reader *json) {
    json->prev = json->curr;
    json->prevValue = json->currValue;
    json->curr = json_read_token(&json->lexer, &json->currValue);
}

static int json_match(struct json_reader *json, int kind) {
    if (json->curr == kind) {
        json_advance(json);
        return PSTRING_TRUE;
    }

    return PSTRING_FALSE;
}

static void json_consume(struct json_reader *json, int kind) {
    if (!json_match(json, kind))
        json->failed = 1;
}

static pf_bool json_expect(struct json_reader *json, int kind) {
    if (!json_match(json, kind)) {
        json->failed = 1;
        return PSTRING_FALSE;
    }

    return PSTRING_TRUE;
}

static struct pstrmodel_member *json_find_member(
    pstring_t *name, const struct pstrmodel *model
) {
    for (size_t i = 0; model->members[i].type; i++)
        if (pstrequals(name, model->members[i].name, 0))
            return &model->members[i];
    return NULL;
}

static void json_skip_value(struct json_reader *json) {
    switch (json->curr) {
    case 'd':
    case 'n':
    case 't':
    case 'f':
    case '"':
        json_advance(json);
        break;

    case '{':
    case '[': {
        char open = json->curr;
        char close = open == '{' ? '}' : ']';

        json_advance(json);
        size_t count = 1;

        while (count > 0) {
            if (json->curr == open)
                count++;
            if (json->curr == close)
                count--;

            json_advance(json);
        }

        break;
    }

    default:
        json->failed = PSTRING_TRUE;
        break;
    }
}

static long double json_consume_number(struct json_reader *json) {
    json_consume(json, 'd');
    return strtold(pstrbuf(&json->prevValue), NULL);
}

static int json_copy_string(struct json_reader *json, pstring_t *out) {
    if (!json_expect(json, '"'))
        return PSTRING_EINVAL;

    if (pstrdec_json(out, &json->prevValue)) {
        json->failed = PSTRING_TRUE;
        return PSTRING_EINVAL;
    }

    return PSTRING_OK;
}

static void json_read_value(
    struct json_reader *json, void *obj, const struct pstrmodel_member *member
) {
    if (json_match(json, 'n'))
        return;

    switch (member->type) {
    case PF_TYPE_CSTRING: {
        pstring_t out = { 0 };

        if (!json_copy_string(json, &out))
            *(char **)obj = pstrunwrap(&out, NULL);
        else
            pstrfree(&out);
        break;
    }

    case PSTRING_TYPE:
        json_copy_string(json, obj);
        break;
    case PSTRING_PTR_TYPE:
        json_copy_string(json, *(pstring_t **)obj);
        break;

    case PSTRMODEL_TYPE:
        pstream_load_json(json->stream, obj, member->model);
        break;

    case PF_TYPE_FLOAT:
        *(float *)obj = json_consume_number(json);
        break;
    case PF_TYPE_DOUBLE:
        *(double *)obj = json_consume_number(json);
        break;
    case PF_TYPE_LDOUBLE:
        *(long double *)obj = json_consume_number(json);
        break;

    case PF_TYPE_CHAR:
    case PF_TYPE_BOOL:
        if (json_match(json, 't') || json_match(json, 'f'))
            *(pf_bool *)obj = (json->prev == 't');
        else
            json->failed = PSTRING_TRUE;
        break;

    default:
        if (pf_type_is_integer(member->type)) {
            uintmax_t value = json_consume_number(json);
            pf_type_int_store(member->type, &value, obj);
            break;
        }

        json_advance(json);
        json->failed = PSTRING_TRUE;
        break;
    }
}

static int json_read_model(
    struct json_reader *json, void *obj, const struct pstrmodel *model
) {
    if (!json_match(json, '{'))
        return PSTRING_EINVAL;

    struct pstrmodel_member *member;
    void *slot;

    while (!json->failed) {
        if (json_match(json, '}'))
            break;

        if (json->prev != '{')
            json_consume(json, ',');

        json_consume(json, '"');
        member = json_find_member(&json->prevValue, model);
        json_consume(json, ':');

        if (member) {
            slot = PF_OFFSET(obj, member->offset);
            json_read_value(json, slot, member);
        } else {
            json_skip_value(json);
        }
    }

    return json->failed;
}

int pstream_load_json(
    pstream_t *stream, void *obj, const struct pstrmodel *model
) {
    if (!stream || !obj || !model || !model->members)
        return PSTRING_EINVAL;

    struct json_reader json;
    json.failed = 0;
    json.lexer.stream = stream;
    json.lexer.eof = 0;
    json.lexer.start = 0;
    json.lexer.end = 0;

    json_advance(&json);
    int result = json_read_model(&json, obj, model);
    /* seek back */
    return result;
}

static int json_read_obj(struct json_reader *json, pstrobj_t *out);

static int obj_copy_string(pstrobj_t *obj, pstring_t *dst, pstring_t *src) {
    if (pstralloc(dst, pstrlen(src), obj->allocator))
        return PSTRING_ENOMEM;

    if (pstrlen(src) == 0)
        return PSTRING_OK;

    if (pstrdec_json(dst, src)) {
        pstrfree(dst);
        return PSTRING_EINVAL;
    }

    return PSTRING_OK;
}

static int json_read_obj_pair(
    struct json_reader *json, pstrobj_t *out, pstrobj_t *child
) {
    pstring_t tmp;
    int result;

    if (!json_expect(json, '"')) /* key */
        return PSTRING_EINVAL;

    if ((result = obj_copy_string(child, &tmp, &json->prevValue)))
        return result;
    pstrobj__set_key(child, &tmp);

    if (pstrobj_copy_key(child, &json->prevValue))
        return PSTRING_ENOMEM;

    json_consume(json, ':');

    if ((result = json_read_obj(json, child)))
        return result;

    if ((result = pstrobj_dict_insert(out, child)))
        return result;

    return PSTRING_OK;
}

static int json_read_obj_index(
    struct json_reader *json, pstrobj_t *out, pstrobj_t *child, size_t index
) {
    int result;

    if ((result = json_read_obj(json, child)))
        return result;

    if ((result = pstrobj_list_insert(out, child, index)))
        return result;

    return PSTRING_OK;
}

static int json_read_obj(struct json_reader *json, pstrobj_t *out) {
    int result;
    json_advance(json);

    switch (json->prev) {
    case '{': {
        pstrobj_t *child;
        pstrobj_set_dict(out);

        while (!json->failed && !json_match(json, '}')) {
            if (out->child != NULL)
                json_consume(json, ',');

            if (!(child = pstrobj_new(out->allocator)))
                return PSTRING_ENOMEM;

            if ((result = json_read_obj_pair(json, out, child))) {
                pstrobj_free(child);
                return result;
            }
        }

        return PSTRING_OK;
    }

    case '[': {
        pstrobj_t *child;
        size_t count = 0;
        pstrobj_set_list(out);

        while (!json->failed && !json_match(json, ']')) {
            if (out->child != NULL)
                json_consume(json, ',');

            if (!(child = pstrobj_new(out->allocator)))
                return PSTRING_ENOMEM;

            if ((result = json_read_obj_index(json, out, child, count++))) {
                pstrobj_free(child);
                return result;
            }
        }

        return PSTRING_OK;
    }

    case '"': {
        pstring_t tmp;

        if ((result = obj_copy_string(out, &tmp, &json->prevValue)))
            return result;

        pstrobj__set_string(out, &tmp);
        return PSTRING_OK;
    }

    case 't':
    case 'f':
        return pstrobj_set_bool(out, json->prev == 't');
    case 'n':
        return pstrobj_set_null(out);

    case 'd': {
        double value = strtold(pstrbuf(&json->prevValue), NULL);
        return pstrobj_set_double(out, value);
    }

    default:
        break;
    }

    return PSTRING_EINVAL;
}

pstrobj_t *pstrobj_load_json(pstream_t *stream, allocator_t *allocator) {
    if (!stream)
        return NULL;

    pstrobj_t *obj;
    struct json_reader json;
    json.failed = 0;
    json.lexer.stream = stream;
    json.lexer.eof = 0;
    json.lexer.start = 0;
    json.lexer.end = 0;

    if (!(obj = pstrobj_new(allocator)))
        return NULL;

    json_advance(&json);
    if (json_read_obj(&json, obj) || json.failed) {
        pstrobj_free(obj);
        return NULL;
    }

    return obj;
}

static int json_write_obj(pstrobj_t *o, pstream_t *s) {
    switch (o->type) {
        /* clang-format off */
    case PSTROBJ_NULL: return pstream_puts(s, "null");
    case PSTROBJ_BOOL: return pstream_puts(s, o->as.bool_ ? "true" : "false");
    case PSTROBJ_LONG: return pstream_printf(s, "%ld", o->as.long_);
    case PSTROBJ_DOUBLE: return pstream_printf(s, "%f", o->as.double_);
        /* clang-format on */

    case PSTROBJ_STRING:
        if (pstrlen(o->as.string) > 0)
            return pstream_printf(s, "\"%!json%P\"", o->as.string);
        else
            return pstream_puts(s, "\"\"");

    case PSTROBJ_LIST: {
        pstrobj_t *child;
        int result = PSTRING_OK;
        pstream_putc(s, '[');

        for (child = o->child; !result && child; child = child->next) {
            if (child != o->child)
                pstream_putc(s, ',');
            result = json_write_obj(child, s);
        }

        pstream_putc(s, ']');
        return result;
    }
    case PSTROBJ_DICT: {
        pstrobj_t *ch;
        int result = PSTRING_OK;
        pstream_putc(s, '{');

        for (ch = o->child; !result && ch; ch = ch->next) {
            if (ch != o->child)
                pstream_putc(s, ',');

            result = pstream_printf(s, "\"%!json%P\":", ch->key)
                || json_write_obj(ch, s);
        }

        pstream_putc(s, '}');
        return result;
    }

    default:
        return PSTRING_EINVAL;
    }
}

int pstrobj_save_json(pstrobj_t *obj, pstream_t *stream) {
    if (!obj || !stream)
        return PSTRING_EINVAL;

    return json_write_obj(obj, stream);
}
