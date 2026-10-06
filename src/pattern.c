/*  pstring - fully-featured string library for C

    Copyright 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/core.h>
#include <pstring/encoding.h>
#include <pstring/pattern.h>
#include <pstring/search.h>
#include <pstring/transform.h>

#include <allocator.h>
#include <allocator_std.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

/* Threads and atomics come from cpolyfill, which maps them to C11, POSIX,
   Windows or GNU C primitives, whichever the platform offers.
   Define PSTREXPR_SINGLE_THREADED to drop all locking.
*/

#define PF_ARRAY_USE_ALLOCATOR_T
#define PF_ARRAY_DEFAULT_ALLOCATOR (&standard_allocator)
#include <pf_array.h>

#ifndef PSTREXPR_SINGLE_THREADED
    #include <pf_threads.h>

    /* pf_atomic.h is only usable through its C11 and GNU C branches, as its
       mutex-based one fails to compile (cpolyfill 1.6.1). Where neither
       applies, reference counts are guarded by the mutex below instead. */
    #if !defined(PF_ATOMIC_NONE)                                      \
        && ((defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
             && !defined(__STDC_NO_ATOMICS__))                        \
            || defined(__GNUC__))
        #include <pf_atomic.h>
    #endif
#endif

#if defined(PSTREXPR_SINGLE_THREADED) || defined(PF_THREADS_NONE)
    #define PSTREXPR_NO_LOCKS
#endif

#ifdef PSTREXPR_NO_LOCKS

static void pat_lock(void) { }
static void pat_unlock(void) { }

#else

static once_flag pat_once = ONCE_FLAG_INIT;
static mtx_t pat_mutex;

static void pat_setup(void) { mtx_init(&pat_mutex, mtx_plain); }

static void pat_lock(void) {
    call_once(&pat_once, pat_setup);
    mtx_lock(&pat_mutex);
}

static void pat_unlock(void) { mtx_unlock(&pat_mutex); }

#endif

#if defined(PF_ATOMIC_STD) || defined(PF_ATOMIC_GNUC)

typedef atomic_size_t pat_refs_t;

    #define REFS_INIT(r, v) atomic_init(&(r), (v))
    #define REFS_INC(r) ((void)atomic_fetch_add(&(r), 1))
    #define REFS_DEC(r) (atomic_fetch_sub(&(r), 1) == 1)

#else

typedef size_t pat_refs_t;

/* Without atomics, the global mutex guards the counts. REFS_INC is only used
   with the lock held, and REFS_DEC never is. */
static int pat_unref(pat_refs_t *refs) {
    pat_lock();
    int last = --*refs == 0;
    pat_unlock();
    return last;
}

    #define REFS_INIT(r, v) ((r) = (v))
    #define REFS_INC(r) ((void)((r)++))
    #define REFS_DEC(r) pat_unref(&(r))

#endif

#define IDENT_CHARS                                                   \
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
#define QUOTE_CHARS " \t\n\r\v\f!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~"
#define DEFAULT_PREFIX "re"
#define PREFIX_MAX 32
#define GROUPS_MAX 65535
#define SCRATCH_SPANS 32
#define SCRATCH_ITEMS 16

struct pstrexpr_t {
    const struct pstrexpr_vt *vt;
    void *user;
    void *prog;
    allocator_t *alloc;
    size_t groups;
    unsigned props;
    pat_refs_t refs;
};

/* Character classes (locale independent) */

static int pat_isalnum(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z');
}

static int pat_isdelim(int c) {
    return c > ' ' && c < 0x7F && !pat_isalnum(c) && c != '_' && c != '\\';
}

static int pat_isescape(int c) {
    return c > ' ' && c < 0x7F && !pat_isalnum(c);
}

/* Engine registry */

typedef struct engine_t {
    pstring_t prefix;
    const struct pstrexpr_vt *vt;
    void *user;
    int alias;
} engine_t;

/* Starts empty, ready to grow with the standard allocator. */
static PF_ARRAY(engine_t) registry = { .base = {
                                           .alloc = &standard_allocator } };

/** Wraps `prefix` in `out` if it's a valid engine name. */
static int pat_prefix(pstring_t *out, const char *prefix) {
    if (!prefix || pstrwrap(out, (char *)prefix, 0, 0))
        return PSTRING_FALSE;

    size_t length = pstrlen(out);
    return length > 0 && length <= PREFIX_MAX
        && pstrspn(out, IDENT_CHARS) == length;
}

/** The caller must hold the lock. */
static engine_t *engine_find(const pstring_t *prefix) {
    for (size_t i = 0; i < PF_ARRAY_LEN(&registry); i++) {
        engine_t *engine = PF_ARRAY_GET(&registry, i);
        if (pstrequal(&engine->prefix, prefix))
            return engine;
    }
    return NULL;
}

/** Adds an engine or replaces an alias. The caller must hold the lock.
    `replaced` is set if an existing alias was rebound.
*/
static int engine_set(
    const pstring_t *prefix,
    const struct pstrexpr_vt *vt,
    void *user,
    int alias,
    int *replaced
) {
    engine_t *found = engine_find(prefix);

    if (found) {
        if (!found->alias)
            return PSTRTHROW(PSTRING_EEXIST, "engine prefix already in use");

        found->vt = vt;
        found->user = user;
        found->alias = alias;
        *replaced = 1;
        return PSTRING_OK;
    }

    engine_t *slot = PF_ARRAY_INCR(&registry, 1);
    if (!slot)
        return PSTRTHROW_ENOMEM;

    if (pstrdup(&slot->prefix, prefix, NULL)) {
        (void)PF_ARRAY_DECR(&registry, 1);
        return PSTRTHROW_ENOMEM;
    }

    slot->vt = vt;
    slot->user = user;
    slot->alias = alias;
    return PSTRING_OK;
}

static int engine_lookup(
    const pstring_t *prefix, const struct pstrexpr_vt **vt, void **user
) {
    pat_lock();
    engine_t *found = engine_find(prefix);
    if (found) {
        *vt = found->vt;
        *user = found->user;
    }
    pat_unlock();

    if (!found)
        return PSTRTHROW(PSTRING_ENOENT, "unknown pattern engine");
    return PSTRING_OK;
}

int pstrexpr_register(
    const char *prefix, const struct pstrexpr_vt *vt, void *user
) {
    pstring_t name;
    if (!pat_prefix(&name, prefix))
        return PSTRTHROW(PSTRING_EINVAL, "invalid engine prefix");

    if (!vt || !vt->compile || !vt->destroy || !vt->exec)
        return PSTRTHROW(PSTRING_EINVAL, "incomplete engine vtable");

    if (vt->escape && !pat_isescape(vt->escape))
        return PSTRTHROW(PSTRING_EINVAL, "invalid engine escape character");

    int replaced = 0;
    pat_lock();

    int rc = engine_set(&name, vt, user, 0, &replaced);
    if (!rc && (vt->flags & PSTREXPR_REGEX)) {
        pstring_t generic;
        pstrwrap(&generic, (char *)DEFAULT_PREFIX, 0, 0);
        if (!engine_find(&generic))
            rc = engine_set(&generic, vt, user, 1, &replaced);
    }

    pat_unlock();

    if (replaced)
        pstrexpr_cache_clear();
    return rc;
}

int pstrexpr_alias(const char *alias, const char *prefix) {
    pstring_t from, to;
    if (!pat_prefix(&from, alias) || !pat_prefix(&to, prefix))
        return PSTRTHROW(PSTRING_EINVAL, "invalid engine prefix");

    int replaced = 0;
    pat_lock();

    int rc = PSTRING_OK;
    engine_t *target = engine_find(&to);
    if (!target) {
        rc = PSTRTHROW(PSTRING_ENOENT, "unknown pattern engine");
    } else {
        /* engine_set may move the registry, so copy what we need */
        const struct pstrexpr_vt *vt = target->vt;
        void *user = target->user;
        rc = engine_set(&from, vt, user, 1, &replaced);
    }

    pat_unlock();

    if (replaced)
        pstrexpr_cache_clear();
    return rc;
}

/* Pattern parsing */

/** Splits `pattern` into the engine, body and flags. The returned slices
    point into `pattern`.
*/
static int pat_parse(
    const char *pattern,
    const struct pstrexpr_vt **vt,
    void **user,
    pstring_t *body,
    pstring_t *flags
) {
    pstring_t all;
    if (!pattern || pstrwrap(&all, (char *)pattern, 0, 0))
        return PSTRTHROW_EINVAL;

    size_t length = pstrlen(&all);
    size_t plen = pstrspn(&all, IDENT_CHARS);
    int delim = (unsigned char)pstrget(&all, plen);

    if (plen > PREFIX_MAX)
        return PSTRTHROW(PSTRING_ENOENT, "unknown pattern engine");
    if (!pat_isdelim(delim))
        return PSTRTHROW(PSTRING_EINVAL, "invalid or missing delimiter");

    pstring_t prefix;
    if (plen > 0)
        pstrslice(&prefix, &all, 0, plen);
    else
        pstrwrap(&prefix, (char *)DEFAULT_PREFIX, 0, 0);

    int rc = engine_lookup(&prefix, vt, user);
    if (rc)
        return rc;

    int escape = (*vt)->escape ? (*vt)->escape : '\\';
    if (delim == escape)
        return PSTRTHROW(PSTRING_EINVAL, "delimiter is the escape character");

    int open = 0;
    int close = delim;
    switch (delim) {
    case '(':
        open = '(', close = ')';
        break;
    case '[':
        open = '[', close = ']';
        break;
    case '{':
        open = '{', close = '}';
        break;
    case '<':
        open = '<', close = '>';
        break;
    }

    const char set[] = { (char)close, (char)escape, (char)open, '\0' };
    size_t pos = plen + 1;
    size_t depth = 0;
    size_t end = 0;

    for (;;) {
        pstring_t rest;
        pstrslice(&rest, &all, pos, length);

        char *found = pstrpbrk(&rest, set);
        if (!found)
            return PSTRTHROW(PSTRING_EINVAL, "missing closing delimiter");

        size_t at = (size_t)(found - pstrbuf(&all));
        int c = (unsigned char)*found;

        if (c == escape) {
            pos = at + 2;
            if (pos > length)
                return PSTRTHROW(PSTRING_EINVAL, "missing closing delimiter");
        } else if (open && c == open) {
            depth++;
            pos = at + 1;
        } else if (depth > 0) {
            depth--;
            pos = at + 1;
        } else {
            end = at;
            break;
        }
    }

    pstrslice(body, &all, plen + 1, end);
    pstrslice(flags, &all, end + 1, length);
    return PSTRING_OK;
}

/* Compiled patterns */

int pstrexpr_compile(
    pstrexpr_t **out, const char *pattern, allocator_t *alloc
) {
    if (!out)
        return PSTRTHROW_EINVAL;
    *out = NULL;

    const struct pstrexpr_vt *vt = NULL;
    void *user = NULL;
    pstring_t body, flags;

    int rc = pat_parse(pattern, &vt, &user, &body, &flags);
    if (rc)
        return rc;

    if (!alloc)
        alloc = &standard_allocator;

    pstrexpr_t *expr = allocate(alloc, sizeof(*expr));
    if (!expr)
        return PSTRTHROW_ENOMEM;

    struct pstrexpr_info info = { 0 };
    expr->prog = NULL;

    rc = vt->compile(&expr->prog, &info, &body, &flags, alloc, user);
    if (rc != PSTRING_OK) {
        deallocate(alloc, expr, sizeof(*expr));
        return rc < 0 ? rc : PSTRING_EINVAL;
    }

    if (info.groups > GROUPS_MAX) {
        vt->destroy(expr->prog, alloc, user);
        deallocate(alloc, expr, sizeof(*expr));
        return PSTRTHROW(PSTRING_EINVAL, "too many capture groups");
    }

    expr->vt = vt;
    expr->user = user;
    expr->alloc = alloc;
    expr->groups = info.groups;
    expr->props = info.props;
    REFS_INIT(expr->refs, 1);

    *out = expr;
    return PSTRING_OK;
}

void pstrexpr_free(pstrexpr_t *expr) {
    if (!expr || !REFS_DEC(expr->refs))
        return;

    allocator_t *alloc = expr->alloc;
    expr->vt->destroy(expr->prog, alloc, expr->user);
    deallocate(alloc, expr, sizeof(*expr));
}

size_t pstrexpr_groups(const pstrexpr_t *expr) {
    return expr ? expr->groups : 0;
}

/* Scratch space for spans, which lives on the stack for typical patterns. */

typedef struct scratch_t {
    pstrexpr_span_t stack[SCRATCH_SPANS];
    pstrexpr_span_t *spans;
    size_t count;
} scratch_t;

static int scratch_init(scratch_t *scratch, size_t count) {
    scratch->count = count;
    scratch->spans = scratch->stack;

    if (count > SCRATCH_SPANS) {
        scratch->spans = allocate(
            &standard_allocator, count * sizeof(pstrexpr_span_t)
        );
        if (!scratch->spans)
            return PSTRTHROW_ENOMEM;
    }
    return PSTRING_OK;
}

static void scratch_free(scratch_t *scratch) {
    if (scratch->spans != scratch->stack)
        deallocate(
            &standard_allocator,
            scratch->spans,
            scratch->count * sizeof(pstrexpr_span_t)
        );
}

/* Output arrays
   A pstrarray_t counts items while pf_array counts bytes, so the two can't
   share memory layout. Instead, an output array is borrowed as a growable
   pf_array for the duration of an operation and handed back by `out_close`.
   Arrays with a buffer but no allocator are caller-owned and never grow. */

typedef PF_ARRAY(pstring_t) items_t;

typedef struct out_t {
    items_t items;
    pstrarray_t *array;
    allocator_t *alloc;
    int fixed;
} out_t;

/** Borrows `array`, discarding its old items without freeing them. */
static void out_open(out_t *out, pstrarray_t *array) {
    out->array = array;
    out->fixed = array->items && !array->allocator;
    out->alloc = array->allocator ? array->allocator : &standard_allocator;

    (void)PF_ARRAY_MAKE(
        &out->items, array->items, 0, array->items ? array->capacity : 0
    );
    out->items.base.alloc = out->alloc;
}

/** Hands the (possibly moved or grown) storage back to the caller's array. */
static void out_close(out_t *out) {
    pstrarray_t *array = out->array;

    array->items = PF_ARRAY_SLOT(&out->items, 0);
    array->capacity = PF_ARRAY_CAP(&out->items);
    array->length = PF_ARRAY_LEN(&out->items);

    if (array->items && !out->fixed)
        array->allocator = out->alloc;
}

/** Discards all items, keeping the storage. */
static void out_clear(out_t *out) { (void)PF_ARRAY_SETLEN(&out->items, 0); }

/** Makes room for `count` more items. */
static int out_reserve(out_t *out, size_t count) {
    size_t length = PF_ARRAY_LEN(&out->items);

    if (out->fixed && length + count > PF_ARRAY_CAP(&out->items))
        return PSTRTHROW(PSTRING_ERANGE, "output array is too small");

    if (count > (SIZE_MAX / 2 - length) / sizeof(pstring_t))
        return PSTRTHROW_ENOMEM;
    if (PF_ARRAY_RESERVE(&out->items, count))
        return PSTRTHROW_ENOMEM;
    return PSTRING_OK;
}

static int out_push(out_t *out, const pstring_t *item) {
    int rc = out_reserve(out, 1);
    if (rc)
        return rc;

    pstring_t copy = *item;
    return PF_ARRAY_PUSH(&out->items, &copy, 1) ? PSTRTHROW_ENOMEM : PSTRING_OK;
}

/** Slices the group described by `span`, or zeroes `out` if it's unset. */
static void slice_span(
    pstring_t *out, const pstring_t *src, const pstrexpr_span_t *span
) {
    if (span->start == PSTREXPR_NOSPAN) {
        *out = (pstring_t) { 0 };
        return;
    }

    size_t length = span->end - span->start;
    pstrwrapb(out, pstrbuf(src) + span->start, length, length);
}

/* Searching */

/** Runs the engine and checks the offsets it returned. */
static int expr_exec(
    const pstrexpr_t *expr,
    const pstring_t *src,
    size_t start,
    pstrexpr_span_t *spans
) {
    size_t count = expr->groups + 1;
    size_t length = pstrlen(src);

    for (size_t i = 0; i < count; i++)
        spans[i] = (pstrexpr_span_t) { PSTREXPR_NOSPAN, PSTREXPR_NOSPAN };

    int rc = expr->vt->exec(
        expr->prog, pstrbuf(src), length, start, spans, count, expr->user
    );

    if (rc < 0)
        return rc;
    if (rc == 0)
        return PSTRING_FALSE;

    int valid = spans[0].start != PSTREXPR_NOSPAN && spans[0].start >= start;
    for (size_t i = 0; valid && i < count; i++) {
        if (spans[i].start == PSTREXPR_NOSPAN) {
            spans[i].end = PSTREXPR_NOSPAN;
            continue;
        }
        valid = spans[i].start <= spans[i].end && spans[i].end <= length;
    }

    if (!valid)
        return PSTRTHROW(PSTRING_EINVAL, "engine returned invalid offsets");
    return PSTRING_TRUE;
}

typedef int(each_fn)(void *ctx, const pstrexpr_span_t *spans);

/** Size of the character at `at`, used to step over empty matches. */
static size_t char_size(
    const pstrexpr_t *expr, const char *base, size_t length, size_t at
) {
    if (at >= length || !(expr->props & PSTREXPR_UTF8))
        return 1;

    uint32_t code = 0;
    const char *next = pstr_read_utf8(base + at, base + length, &code);
    return next > base + at ? (size_t)(next - (base + at)) : 1;
}

/** Calls `fn` for all matches, at most `max` if it's not zero. The callback
    returns `0` to continue, a positive value to stop or a negative error.
    Returns the number of matches visited or a negative error.
*/
static int expr_each(
    const pstrexpr_t *expr,
    const pstring_t *src,
    size_t max,
    each_fn *fn,
    void *ctx
) {
    scratch_t scratch;
    int rc = scratch_init(&scratch, expr->groups + 1);
    if (rc)
        return rc;

    const char *base = pstrbuf(src);
    size_t length = pstrlen(src);
    size_t pos = 0;
    size_t count = 0;
    int result = PSTRING_OK;

    while (pos <= length && (max == 0 || count < max)) {
        rc = expr_exec(expr, src, pos, scratch.spans);
        if (rc < 0) {
            result = rc;
            break;
        }
        if (rc == 0)
            break;

        count++;
        rc = fn(ctx, scratch.spans);
        if (rc < 0) {
            result = rc;
            break;
        }
        if (rc > 0)
            break;

        size_t start = scratch.spans[0].start;
        size_t end = scratch.spans[0].end;
        pos = end > start ? end : end + char_size(expr, base, length, end);
    }

    scratch_free(&scratch);
    if (result < 0)
        return result;
    return count > INT_MAX ? INT_MAX : (int)count;
}

/* Matching */

int pstrexpr_match(
    const pstrexpr_t *expr, const pstring_t *src, pstrarray_t *captures
) {
    if (!expr || !src)
        return PSTRTHROW_EINVAL;

    scratch_t scratch;
    int rc = scratch_init(&scratch, expr->groups + 1);
    if (rc)
        return rc;

    rc = expr_exec(expr, src, 0, scratch.spans);

    if (captures && rc >= 0) {
        out_t out;
        out_open(&out, captures);

        if (rc > 0) {
            int err = out_reserve(&out, scratch.count);
            pstring_t *slots = err ? NULL
                                   : PF_ARRAY_INCR(&out.items, scratch.count);

            if (slots) {
                for (size_t i = 0; i < scratch.count; i++)
                    slice_span(&slots[i], src, &scratch.spans[i]);
            } else {
                rc = err ? err : PSTRTHROW_ENOMEM;
            }
        }

        out_close(&out);
    }

    scratch_free(&scratch);
    return rc;
}

/* Splitting */

typedef struct split_ctx {
    out_t *out;
    const pstring_t *src;
    size_t piece;
    size_t length;
    size_t splits;
    size_t max;
} split_ctx_t;

static int split_push(split_ctx_t *ctx, size_t from, size_t to) {
    pstring_t part;
    pstrwrapb(&part, pstrbuf(ctx->src) + from, to - from, to - from);
    return out_push(ctx->out, &part);
}

static int split_each(void *data, const pstrexpr_span_t *spans) {
    split_ctx_t *ctx = data;
    size_t start = spans[0].start;
    size_t end = spans[0].end;

    /* empty matches at the edges of a piece don't separate anything */
    if (start == end && (start == ctx->piece || start >= ctx->length))
        return 0;

    int rc = split_push(ctx, ctx->piece, start);
    if (rc)
        return rc;

    ctx->piece = end;
    ctx->splits++;
    return ctx->max && ctx->splits >= ctx->max;
}

int pstrexpr_split(
    const pstrexpr_t *expr,
    const pstring_t *src,
    pstrarray_t *parts,
    size_t maxSplit
) {
    if (!expr || !src || !parts)
        return PSTRTHROW_EINVAL;

    out_t out;
    out_open(&out, parts);

    split_ctx_t ctx = {
        .out = &out,
        .src = src,
        .length = pstrlen(src),
        .max = maxSplit,
    };

    int rc = expr_each(expr, src, 0, split_each, &ctx);
    if (rc >= 0)
        rc = split_push(&ctx, ctx.piece, ctx.length);

    size_t count = PF_ARRAY_LEN(&out.items);
    if (rc < 0)
        out_clear(&out);

    out_close(&out);
    if (rc < 0)
        return rc;

    return count > INT_MAX ? INT_MAX : (int)count;
}

/* Replacing */

/** Parses the replacement template, appending the result to `out` if it's not
    `NULL`. With `spans` set to `NULL` it only checks that the template is
    valid, which is done before touching the string.
*/
static int repl_expand(
    const pstrexpr_t *expr,
    const char *repl,
    const pstring_t *src,
    const pstrexpr_span_t *spans,
    pstring_t *out
) {
    pstring_t all;
    if (pstrwrap(&all, (char *)repl, 0, 0))
        return PSTRTHROW_EINVAL;

    const char *base = pstrbuf(&all);
    size_t length = pstrlen(&all);
    size_t pos = 0;
    int rc;

    while (pos < length) {
        pstring_t rest;
        pstrslice(&rest, &all, pos, length);

        char *dollar = pstrchr(&rest, '$');
        size_t run = dollar ? (size_t)(dollar - (base + pos)) : length - pos;

        if (out && (rc = pstrcatb(out, base + pos, run)))
            return rc;

        pos += run;
        if (!dollar)
            break;

        pos++;
        int c = (unsigned char)pstrget(&all, pos);
        size_t group = 0;

        if (c == '$') {
            if (out && (rc = pstrcatc(out, '$')))
                return rc;
            pos++;
            continue;
        } else if (c >= '0' && c <= '9') {
            group = (size_t)(c - '0');
            pos++;
        } else if (c == '{') {
            pstrslice(&rest, &all, pos + 1, length);

            char *close = pstrchr(&rest, '}');
            if (!close)
                return PSTRTHROW(PSTRING_EINVAL, "unterminated ${...}");

            size_t nlen = (size_t)(close - (base + pos + 1));
            if (nlen == 0)
                return PSTRTHROW(PSTRING_EINVAL, "empty ${} reference");

            pstring_t name;
            pstrslice(&name, &all, pos + 1, pos + 1 + nlen);
            pos += nlen + 2;

            if (pstrget(&name, 0) >= '0' && pstrget(&name, 0) <= '9') {
                for (size_t i = 0; i < nlen; i++) {
                    c = (unsigned char)pstrget(&name, i);
                    if (c < '0' || c > '9' || group > GROUPS_MAX)
                        return PSTRTHROW(PSTRING_EINVAL, "bad group number");
                    group = group * 10 + (size_t)(c - '0');
                }
            } else {
                if (!expr->vt->group)
                    return PSTRTHROW(
                        PSTRING_EINVAL, "engine has no named groups"
                    );

                rc = expr->vt->group(expr->prog, &name, expr->user);
                if (rc < 0)
                    return rc;
                group = (size_t)rc;
            }
        } else {
            return PSTRTHROW(
                PSTRING_EINVAL, "invalid reference in replacement"
            );
        }

        if (group > expr->groups)
            return PSTRTHROW(
                PSTRING_EINVAL, "replacement uses a missing group"
            );

        if (out && spans && spans[group].start != PSTREXPR_NOSPAN) {
            const char *text = pstrbuf(src) + spans[group].start;
            size_t size = spans[group].end - spans[group].start;
            if ((rc = pstrcatb(out, text, size)))
                return rc;
        }
    }

    return PSTRING_OK;
}

typedef struct repl_ctx {
    const pstrexpr_t *expr;
    const char *repl;
    const pstring_t *src;
    pstring_t *out;
    size_t last;
} repl_ctx_t;

static int repl_each(void *data, const pstrexpr_span_t *spans) {
    repl_ctx_t *ctx = data;

    int rc = pstrcatb(
        ctx->out, pstrbuf(ctx->src) + ctx->last, spans[0].start - ctx->last
    );
    if (rc)
        return rc;

    ctx->last = spans[0].end;
    return repl_expand(ctx->expr, ctx->repl, ctx->src, spans, ctx->out);
}

int pstrexpr_repl(
    const pstrexpr_t *expr, const char *repl, pstring_t *str, size_t max
) {
    if (!expr || !repl || !str)
        return PSTRTHROW_EINVAL;
    if (!pstrowned(str))
        return PSTRTHROW(PSTRING_EINVAL, "cannot modify a slice");

    int rc = repl_expand(expr, repl, str, NULL, NULL);
    if (rc)
        return rc;

    pstring_t out;
    if (pstralloc(&out, pstrlen(str), pstrallocator(str)))
        return PSTRING_ENOMEM;

    repl_ctx_t ctx = { expr, repl, str, &out, 0 };
    rc = expr_each(expr, str, max, repl_each, &ctx);

    if (rc > 0) {
        int err = pstrcatb(
            &out, pstrbuf(str) + ctx.last, pstrlen(str) - ctx.last
        );
        if (err)
            rc = err;
    }

    if (rc <= 0) {
        pstrfree(&out);
        return rc;
    }

    pstrfree(str);
    *str = out;
    return rc;
}

/* Iterating */

typedef struct iter_ctx {
    const pstring_t *src;
    pstriterp_fn *fn;
    void *user;
    pstring_t *items;
    size_t count;
    size_t index;
} iter_ctx_t;

static int iter_each(void *data, const pstrexpr_span_t *spans) {
    iter_ctx_t *ctx = data;

    for (size_t i = 0; i < ctx->count; i++)
        slice_span(&ctx->items[i], ctx->src, &spans[i]);

    pstrarray_t view = {
        .capacity = ctx->count,
        .length = ctx->count,
        .items = ctx->items,
        .allocator = NULL,
    };
    return ctx->fn(&view, ctx->index++, ctx->user);
}

int pstrexpr_iter(
    const pstrexpr_t *expr, const pstring_t *src, pstriterp_fn *iter, void *user
) {
    if (!expr || !src || !iter)
        return PSTRTHROW_EINVAL;

    pstring_t stack[SCRATCH_ITEMS];
    iter_ctx_t ctx = { src, iter, user, stack, expr->groups + 1, 0 };

    if (ctx.count > SCRATCH_ITEMS) {
        ctx.items = allocate(
            &standard_allocator, ctx.count * sizeof(pstring_t)
        );
        if (!ctx.items)
            return PSTRTHROW_ENOMEM;
    }

    int rc = expr_each(expr, src, 0, iter_each, &ctx);

    if (ctx.items != stack)
        deallocate(
            &standard_allocator, ctx.items, ctx.count * sizeof(pstring_t)
        );
    return rc;
}

/* Escaping */

static int quote_default(pstring_t *out, const pstring_t *text, int escape) {
    const char *base = pstrbuf(text);
    size_t length = pstrlen(text);
    size_t pos = 0;
    int rc;

    while (pos < length) {
        pstring_t rest;
        pstrslice(&rest, text, pos, length);

        char *special = pstrpbrk(&rest, QUOTE_CHARS);
        size_t run = special ? (size_t)(special - (base + pos)) : length - pos;

        if ((rc = pstrcatb(out, base + pos, run)))
            return rc;
        if (!special)
            break;

        if ((rc = pstrcatc(out, (char)escape))
            || (rc = pstrcatc(out, *special)))
            return rc;
        pos += run + 1;
    }

    return PSTRING_OK;
}

/** Finds the engine named by `prefix`, which may be `NULL` or empty. */
static int engine_resolve(
    const char *prefix, const struct pstrexpr_vt **vt, void **user
) {
    pstring_t name;
    if (!prefix || !*prefix)
        prefix = DEFAULT_PREFIX;
    if (!pat_prefix(&name, prefix))
        return PSTRTHROW(PSTRING_EINVAL, "invalid engine prefix");

    return engine_lookup(&name, vt, user);
}

static int pat_escape(
    const struct pstrexpr_vt *vt,
    void *user,
    pstring_t *out,
    const pstring_t *text
) {
    if (vt->quote)
        return vt->quote(out, text, user);
    return quote_default(out, text, vt->escape ? vt->escape : '\\');
}

int pstrexpr_escape(pstring_t *out, const char *prefix, const pstring_t *text) {
    if (!out || !text)
        return PSTRTHROW_EINVAL;

    const struct pstrexpr_vt *vt = NULL;
    void *user = NULL;
    int rc = engine_resolve(prefix, &vt, &user);
    if (rc)
        return rc;

    return pat_escape(vt, user, out, text);
}

int pstrexpr_quote(
    pstring_t *out, const char *prefix, const pstring_t *text, const char *flags
) {
    if (!out || !text)
        return PSTRTHROW_EINVAL;

    const struct pstrexpr_vt *vt = NULL;
    void *user = NULL;
    int rc = engine_resolve(prefix, &vt, &user);
    if (rc)
        return rc;

    pstring_t body = { 0 };
    if ((rc = pat_escape(vt, user, &body, text))) {
        pstrfree(&body);
        return rc;
    }

    /* The default escaping protects every punctuation character, delimiter
       included, so any will do. Engines with their own quoting may leave it
       alone, so pick one that doesn't occur in the body at all. */
    static const char candidates[] = "/|#!~@:;,=&%+-^$.*?\"'`";
    int escape = vt->escape ? vt->escape : '\\';
    int delim = 0;

    for (size_t i = 0; candidates[i] && !delim; i++) {
        int c = (unsigned char)candidates[i];
        if (!pat_isdelim(c) || c == escape)
            continue;
        if (!vt->quote || !pstrchr(&body, c))
            delim = c;
    }

    if (!delim) {
        pstrfree(&body);
        return PSTRTHROW(PSTRING_ERANGE, "no usable delimiter");
    }

    size_t length = pstrlen(out);

    rc = PSTRING_OK;
    if (prefix && *prefix)
        rc = pstrcats(out, prefix, 0);
    if (!rc)
        rc = pstrcatc(out, (char)delim);
    if (!rc)
        rc = pstrcat(out, &body);
    if (!rc)
        rc = pstrcatc(out, (char)delim);
    if (!rc && flags && *flags)
        rc = pstrcats(out, flags, 0);

    if (rc)
        pstr__setlen(out, length);

    pstrfree(&body);
    return rc;
}

/* Cache of compiled patterns */

typedef struct cache_slot_t {
    pstring_t key;
    pstrexpr_t *expr;
    unsigned long stamp;
} cache_slot_t;

static cache_slot_t cache[PSTREXPR_CACHE_SIZE];
static unsigned long cache_clock;

/** Returns a new reference to the cached pattern. Needs the lock. */
static pstrexpr_t *cache_find(const char *pattern, size_t length) {
    for (size_t i = 0; i < PSTREXPR_CACHE_SIZE; i++) {
        cache_slot_t *slot = &cache[i];
        if (slot->expr && pstrequalb(&slot->key, pattern, length)) {
            slot->stamp = ++cache_clock;
            REFS_INC(slot->expr->refs);
            return slot->expr;
        }
    }
    return NULL;
}

/** Returns a new reference to the compiled pattern, to be released using
    `pstrexpr_free`.
*/
static int cache_acquire(const char *pattern, pstrexpr_t **out) {
    if (!pattern || !*pattern)
        return PSTRTHROW_EINVAL;

    size_t length = strlen(pattern);

    pat_lock();
    pstrexpr_t *expr = cache_find(pattern, length);
    pat_unlock();

    if (expr) {
        *out = expr;
        return PSTRING_OK;
    }

    /* compile without holding the lock */
    int rc = pstrexpr_compile(&expr, pattern, NULL);
    if (rc)
        return rc;

    pstring_t key;
    int keyed = !pstrnew(&key, pattern, length, NULL);
    pstrexpr_t *found = NULL;
    pstrexpr_t *evicted = NULL;

    pat_lock();
    found = cache_find(pattern, length);

    if (!found && keyed) {
        cache_slot_t *slot = &cache[0];
        for (size_t i = 0; i < PSTREXPR_CACHE_SIZE; i++) {
            if (!cache[i].expr) {
                slot = &cache[i];
                break;
            }
            if (cache[i].stamp < slot->stamp)
                slot = &cache[i];
        }

        if (slot->expr) {
            evicted = slot->expr;
            pstrfree(&slot->key);
        }

        slot->key = key;
        slot->expr = expr;
        slot->stamp = ++cache_clock;
        REFS_INC(expr->refs);
        keyed = 0;
    }
    pat_unlock();

    if (keyed)
        pstrfree(&key);
    pstrexpr_free(evicted);

    if (found) {
        pstrexpr_free(expr);
        expr = found;
    }

    *out = expr;
    return PSTRING_OK;
}

void pstrexpr_cache_clear(void) {
    pstrexpr_t *dropped[PSTREXPR_CACHE_SIZE];
    size_t count = 0;

    pat_lock();
    for (size_t i = 0; i < PSTREXPR_CACHE_SIZE; i++) {
        if (cache[i].expr) {
            dropped[count++] = cache[i].expr;
            pstrfree(&cache[i].key);
            cache[i].expr = NULL;
        }
    }
    pat_unlock();

    for (size_t i = 0; i < count; i++)
        pstrexpr_free(dropped[i]);
}

int pstrmatch(
    const char *pattern, const pstring_t *src, pstrarray_t *captures
) {
    pstrexpr_t *expr;
    int rc = cache_acquire(pattern, &expr);
    if (rc)
        return rc;

    rc = pstrexpr_match(expr, src, captures);
    pstrexpr_free(expr);
    return rc;
}

int pstrsplitp(
    const char *pattern,
    const pstring_t *src,
    pstrarray_t *parts,
    size_t maxSplit
) {
    pstrexpr_t *expr;
    int rc = cache_acquire(pattern, &expr);
    if (rc)
        return rc;

    rc = pstrexpr_split(expr, src, parts, maxSplit);
    pstrexpr_free(expr);
    return rc;
}

int pstrreplp(
    const char *pattern, const char *repl, pstring_t *str, size_t max
) {
    pstrexpr_t *expr;
    int rc = cache_acquire(pattern, &expr);
    if (rc)
        return rc;

    rc = pstrexpr_repl(expr, repl, str, max);
    pstrexpr_free(expr);
    return rc;
}

int pstriterp(
    const char *pattern, const pstring_t *src, pstriterp_fn *iter, void *user
) {
    pstrexpr_t *expr;
    int rc = cache_acquire(pattern, &expr);
    if (rc)
        return rc;

    rc = pstrexpr_iter(expr, src, iter, user);
    pstrexpr_free(expr);
    return rc;
}
