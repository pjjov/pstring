/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/encoding.h>
#include <pstring/io.h>
#include <pstring/object.h>
#include <pstring/pstring.h>

#include <pf_io.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
   writer
   ================================================================== */

static int write_value(pf_stream_t *s, pstrobj_t *obj);

static int is_valid_name(pstring_t *key) {
    if (!key || pstrlen(key) == 0)
        return 0;

    char first = pstrget(key, 0);
    if (!(isalpha((unsigned char)first) || first == '_'))
        return 0;

    for (size_t i = 0; i < pstrlen(key); i++) {
        char c = pstrget(key, i);
        if (!(isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.'))
            return 0;
    }

    return 1;
}

static int write_children(pf_stream_t *s, pstrobj_t *obj) {
    pstrobj_t *child;
    int rc = PSTRING_OK;

    PSTROBJ_FOREACH(obj, child) {
        int usedFallback = !is_valid_name(child->key);

        if (usedFallback) {
            if ((rc = pstrfprintf(s, "<field key=\"%!xml%P\">", child->key)))
                break;
        } else {
            if ((rc = pf_stream_putc(s, '<') || pf_stream_putp(s, child->key)
                     || pf_stream_putc(s, '>')))
                break;
        }

        if ((rc = write_value(s, child)))
            break;

        if (usedFallback)
            rc = pf_stream_puts(s, "</field>");
        else {
            rc = pf_stream_puts(s, "</") || pf_stream_putp(s, child->key)
                || pf_stream_putc(s, '>');
        }

        if (rc)
            break;
    }

    return rc;
}

static int write_value(pf_stream_t *s, pstrobj_t *obj) {
    if (!obj)
        return PSTRING_OK;

    switch (obj->type) {
    case PSTROBJ_NULL:
        return PSTRING_OK;

    case PSTROBJ_BOOL:
        return pf_stream_puts(s, obj->as.bool_ ? "true" : "false");

    case PSTROBJ_LONG:
        return pf_stream_printf(s, "%ld", obj->as.long_);

    case PSTROBJ_DOUBLE:
        return pf_stream_printf(s, "%.17g", obj->as.double_);

    case PSTROBJ_STRING:
        return pstrfprintf(s, "%!xml%P", obj->as.string);

    case PSTROBJ_LIST: {
        pstrobj_t *child;
        int rc = PSTRING_OK;

        PSTROBJ_FOREACH(obj, child) {
            if ((rc = pf_stream_puts(s, "<item>") || write_value(s, child)
                     || pf_stream_puts(s, "</item>")))
                break;
        }

        return rc;
    }

    case PSTROBJ_DICT:
        return write_children(s, obj);
    }

    return PSTRTHROW_EINVAL;
}

int pstrobj_save_xml(pstrobj_t *obj, pf_stream_t *stream) {
    if (!obj || !stream)
        return PSTRTHROW_EINVAL;

    int rc = pf_stream_puts(
                 stream, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
             )
        || pf_stream_puts(stream, "<root>") || write_value(stream, obj)
        || pf_stream_puts(stream, "</root>");

    return rc ? PSTRTHROW(rc, NULL) : PSTRING_OK;
}

/* ==================================================================
   reader

   The whole document is read into memory up front and parsed with a
   small hand-written recursive-descent scanner over that buffer. XML
   documents modelled as pstrobj trees are not expected to be huge (the
   same assumption format/json.c's own reader makes for a single JSON
   document), so this trades true incremental streaming for a much
   simpler and easier to get right implementation.
   ================================================================== */

struct xml_parser {
    const char *p;
    const char *end;
    allocator_t *alloc;
    int error;
};

static void xml_skip_ws(struct xml_parser *x) {
    while (x->p < x->end && isspace((unsigned char)*x->p))
        x->p++;
}

static int xml_starts_with(struct xml_parser *x, const char *tok) {
    size_t len = strlen(tok);
    return (size_t)(x->end - x->p) >= len && 0 == memcmp(x->p, tok, len);
}

static void xml_skip_until(struct xml_parser *x, const char *tok) {
    size_t len = strlen(tok);
    while (x->p < x->end && !xml_starts_with(x, tok))
        x->p++;
    if (x->p < x->end)
        x->p += len;
    else
        x->error = PSTRING_EINVAL; /* unterminated comment/PI/doctype */
}

/** Skips whitespace, XML/PHP-style processing instructions (`<?...?>`),
    comments (`<!--...-->`), and the doctype declaration (`<!...>`,
    tracking bracket depth for an internal subset) -- anything that can
    legally appear before or between elements but isn't itself part of
    the object model. **/
static void xml_skip_misc(struct xml_parser *x) {
    for (;;) {
        xml_skip_ws(x);

        if (xml_starts_with(x, "<?")) {
            xml_skip_until(x, "?>");
        } else if (xml_starts_with(x, "<!--")) {
            xml_skip_until(x, "-->");
        } else if (xml_starts_with(x, "<!")) {
            int depth = 0;
            const char *start = x->p;
            x->p += 2;
            while (x->p < x->end) {
                if (*x->p == '[')
                    depth++;
                else if (*x->p == ']')
                    depth--;
                else if (*x->p == '>' && depth <= 0) {
                    x->p++;
                    break;
                }
                x->p++;
            }
            if (x->p >= x->end && start != x->p)
                x->error = PSTRING_EINVAL;
        } else {
            break;
        }

        if (x->error)
            break;
    }
}

static int is_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_' || c == ':';
}

static int is_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == ':' || c == '-'
        || c == '.';
}

static void xml_read_name(
    struct xml_parser *x, const char **start, const char **stop
) {
    *start = x->p;
    if (x->p < x->end && is_name_start(*x->p))
        x->p++;
    while (x->p < x->end && is_name_char(*x->p))
        x->p++;
    *stop = x->p;
}

/** Skips an attribute list up to the tag's closing `>` (or `/>`),
    respecting quoted attribute values so a `>` inside e.g.
    `alt=">"` doesn't end the tag early. Attribute values themselves
    aren't modelled -- pstrobj has no representation for them -- so
    they're intentionally discarded rather than parsed. Returns 1 if
    the tag is self-closing. **/
static int xml_skip_attrs(struct xml_parser *x) {
    char quote = 0;

    while (x->p < x->end) {
        char c = *x->p;

        if (quote) {
            if (c == quote)
                quote = 0;
            x->p++;
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            x->p++;
            continue;
        }

        if (c == '>') {
            int selfClose = (x->p > (x->p - 1)) && x->p[-1] == '/';
            x->p++;
            return selfClose;
        }

        x->p++;
    }

    x->error = PSTRING_EINVAL; /* unterminated tag */
    return 0;
}

static pstrobj_t *xml_new(struct xml_parser *x) {
    pstrobj_t *obj = pstrobj_new(x->alloc);
    if (!obj)
        x->error = PSTRING_ENOMEM;
    return obj;
}

/** Infers a scalar type from an element's trimmed text content, the
    same way `format/json.c`'s number handling does: no digits/sign/dot
    at all falls through to a plain string, so this never misclassifies
    ordinary text as a number by accident. **/
static int xml_set_scalar(
    struct xml_parser *x, pstrobj_t *obj, pstring_t *text
) {
    (void)x;
    size_t len = pstrlen(text);

    if (len == 0)
        return pstrobj_set_null(obj);

    if (len == 4 && 0 == memcmp(pstrbuf(text), "true", 4))
        return pstrobj_set_bool(obj, 1);
    if (len == 5 && 0 == memcmp(pstrbuf(text), "false", 5))
        return pstrobj_set_bool(obj, 0);

    int isNumeric = 1, isFloat = 0;
    for (size_t i = 0; i < len && isNumeric; i++) {
        char c = pstrget(text, i);
        if (c == '-' && i == 0)
            continue;
        if (c == '.' || c == 'e' || c == 'E' || c == '+') {
            isFloat = 1;
            continue;
        }
        if (!isdigit((unsigned char)c))
            isNumeric = 0;
    }

    if (isNumeric) {
        char buf[64];
        const char *cstr = pstrterms(text, buf, sizeof(buf));
        if (cstr) {
            char *stop;
            if (isFloat) {
                double v = strtod(cstr, &stop);
                if (*stop == '\0')
                    return pstrobj_set_double(obj, v);
            } else {
                long v = strtol(cstr, &stop, 10);
                if (*stop == '\0')
                    return pstrobj_set_long(obj, v);
            }
        }
        /* fell through: didn't fully parse (e.g. "1.2.3") -- treat as
           text after all instead of failing the whole document */
    }

    pstring_t decoded = { 0 };
    if (pstrdec_xml(&decoded, text))
        return PSTRING_EINVAL;

    int rc = pstrobj_copy_pstring(obj, &decoded);
    pstrfree(&decoded);
    return rc;
}

static pstrobj_t *xml_parse_element(struct xml_parser *x, pstring_t *outName);

/** Parses the content of an already-opened, non-self-closing element:
    a run of child elements and/or text, up to (but not including) its
    closing tag. Decides the resulting object's shape from what it
    found -- no children at all means a scalar leaf; every child named
    "item" means a list; anything else means a dict (a child using the
    `<field key="...">` escape from the writer contributes its `key`
    attribute as the dict key instead of its tag name). **/
static pstrobj_t *xml_parse_content(struct xml_parser *x) {
    pstring_t text = { 0 };
    pstrobj_t *list = NULL;
    pstrobj_t *dict = NULL;
    size_t index = 0;
    int allItems = 1;
    int anyChild = 0;

    while (!x->error && x->p < x->end && *x->p != '<') {
        const char *start = x->p;
        while (x->p < x->end && *x->p != '<')
            x->p++;
        pstrcats(&text, start, x->p - start);
    }

    while (!x->error && x->p < x->end && *x->p == '<' && x->p[1] != '/') {
        pstring_t rawName;
        pstrwrap(&rawName, NULL, 0, 0);
        pstrobj_t *child = xml_parse_element(x, &rawName);

        if (x->error) {
            pstrfree(&rawName);
            break;
        }

        anyChild = 1;

        int isItem = pstrlen(&rawName) == 4
            && 0 == memcmp(pstrbuf(&rawName), "item", 4);
        int isField = pstrlen(&rawName) == 5
            && 0 == memcmp(pstrbuf(&rawName), "field", 5);

        if (isItem && allItems) {
            if (!list) {
                list = xml_new(x);
                if (list)
                    pstrobj_set_list(list);
            }
            if (list && !x->error)
                x->error = pstrobj_list_insert(list, child, index++);
        } else {
            allItems = 0;
            if (!dict) {
                dict = xml_new(x);
                if (dict)
                    pstrobj_set_dict(dict);
                /* an "item"-only run seen so far becomes a dict too:
                   move it under a synthetic numeric-ish key so no data
                   is silently dropped if a document mixes styles */
                if (list) {
                    pstrobj_t *item;
                    size_t i = 0;
                    while ((item = pstrobj_list_remove(list, 0))) {
                        char key[24];
                        int n = snprintf(key, sizeof(key), "item%zu", i++);
                        pstrobj_copy_keys(item, key, (size_t)n);
                        if (dict)
                            pstrobj_dict_insert(dict, item);
                    }
                    pstrobj_free(list);
                    list = NULL;
                }
            }

            if (dict) {
                if (isField) {
                    /* key already attached to `child` by
                       xml_parse_element via the "key" attribute path;
                       nothing further to do here */
                } else {
                    pstrobj_copy_keys(
                        child, pstrbuf(&rawName), pstrlen(&rawName)
                    );
                }
                if (!x->error)
                    x->error = pstrobj_dict_insert(dict, child);
            }
        }

        pstrfree(&rawName);

        while (!x->error && x->p < x->end && *x->p != '<') {
            const char *start = x->p;
            while (x->p < x->end && *x->p != '<')
                x->p++;
            pstrcats(&text, start, x->p - start);
        }
    }

    pstrobj_t *result = NULL;

    if (!x->error) {
        if (anyChild) {
            result = list ? list : dict;
            if (!result) {
                /* shouldn't happen, but don't leak `text` if it does */
                result = xml_new(x);
                if (result)
                    pstrobj_set_null(result);
            }
        } else {
            result = xml_new(x);
            if (result) {
                pstrlstrip(&text, " \t\r\n");
                pstrrstrip(&text, " \t\r\n");
                x->error = xml_set_scalar(x, result, &text);
            }
        }
    }

    pstrfree(&text);

    if (x->error) {
        if (list)
            pstrobj_free(list);
        if (dict)
            pstrobj_free(dict);
        if (result && result != list && result != dict)
            pstrobj_free(result);
        return NULL;
    }

    return result;
}

/** Parses one element starting at `x->p == '<'`: reads the tag name
    into `*outName` (used by the caller to recognise "item"/"field"),
    handles the `<field key="...">` escape hatch the writer uses for
    dict keys that aren't valid XML names, recurses into the element's
    content, and consumes the matching closing tag. **/
static pstrobj_t *xml_parse_element(struct xml_parser *x, pstring_t *outName) {
    x->p++; /* '<' */

    const char *nameStart, *nameEnd;
    xml_read_name(x, &nameStart, &nameEnd);

    if (nameStart == nameEnd) {
        x->error = PSTRING_EINVAL;
        return NULL;
    }

    pstrwrap(outName, (char *)nameStart, (size_t)(nameEnd - nameStart), 0);

    int isField = pstrlen(outName) == 5
        && 0 == memcmp(pstrbuf(outName), "field", 5);

    pstring_t keyAttr = { 0 };
    int haveKeyAttr = 0;

    if (isField) {
        /* look for key="..." (or key='...') among the attributes; any
           other attribute is still skipped normally below */
        xml_skip_ws(x);
        while (x->p < x->end && *x->p != '>' && *x->p != '/') {
            const char *attrStart, *attrEnd;
            xml_read_name(x, &attrStart, &attrEnd);
            if (attrStart == attrEnd)
                break;

            xml_skip_ws(x);
            int isKeyAttr = (attrEnd - attrStart == 3)
                && 0 == memcmp(attrStart, "key", 3);

            if (x->p < x->end && *x->p == '=') {
                x->p++;
                xml_skip_ws(x);
                if (x->p < x->end && (*x->p == '"' || *x->p == '\'')) {
                    char q = *x->p++;
                    const char *valStart = x->p;
                    while (x->p < x->end && *x->p != q)
                        x->p++;
                    if (isKeyAttr && !haveKeyAttr) {
                        pstring_t raw;
                        pstrrange(&raw, NULL, valStart, x->p);
                        if (!pstrdec_xml(&keyAttr, &raw))
                            haveKeyAttr = 1;
                    }
                    if (x->p < x->end)
                        x->p++; /* closing quote */
                }
            }
            xml_skip_ws(x);
        }
    }

    int selfClose = xml_skip_attrs(x);
    if (x->error) {
        pstrfree(&keyAttr);
        return NULL;
    }

    pstrobj_t *obj;

    if (selfClose) {
        obj = xml_new(x);
        if (obj)
            x->error = pstrobj_set_null(obj);
    } else {
        obj = xml_parse_content(x);

        if (!x->error) {
            /* expect the matching closing tag; the name isn't
               re-validated against `outName` (a generic recursive
               descent already guarantees correct nesting depth) */
            if (x->p + 1 < x->end && x->p[0] == '<' && x->p[1] == '/') {
                x->p += 2;
                const char *closeStart, *closeEnd;
                xml_read_name(x, &closeStart, &closeEnd);
                xml_skip_ws(x);
                if (x->p < x->end && *x->p == '>')
                    x->p++;
                else
                    x->error = PSTRING_EINVAL;
                (void)closeStart;
                (void)closeEnd;
            } else {
                x->error = PSTRING_EINVAL;
            }
        }
    }

    if (haveKeyAttr && obj && !x->error) {
        int rc = pstrobj_copy_key(obj, &keyAttr);
        if (rc)
            x->error = rc;
    }
    pstrfree(&keyAttr);

    if (x->error && obj) {
        pstrobj_free(obj);
        return NULL;
    }

    return obj;
}

pstrobj_t *pstrobj_load_xml(pf_stream_t *stream, allocator_t *allocator) {
    if (!stream)
        return PSTRTHROW_NULL(PSTRING_EINVAL);

    pstring_t buffer = { 0 };
    char chunk[4096];
    size_t n;

    while ((n = pf_stream_read(stream, chunk, sizeof(chunk))) > 0) {
        if (pstrcats(&buffer, chunk, n)) {
            pstrfree(&buffer);
            return PSTRTHROW_NULL(PSTRING_ENOMEM);
        }
    }

    struct xml_parser x = {
        .p = pstrbuf(&buffer),
        .end = pstrend(&buffer),
        .alloc = allocator,
        .error = 0,
    };

    xml_skip_misc(&x);

    pstrobj_t *result = NULL;

    if (!x.error && x.p < x.end && *x.p == '<') {
        pstring_t rootName;
        result = xml_parse_element(&x, &rootName);
        pstrfree(&rootName);
    } else if (!x.error) {
        x.error = PSTRING_EINVAL; /* no root element at all */
    }

    xml_skip_misc(&x); /* trailing comments/whitespace are fine */

    pstrfree(&buffer);

    if (x.error) {
        if (result)
            pstrobj_free(result);
        return PSTRTHROW_NULL(x.error);
    }

    return result;
}
