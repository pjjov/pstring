/*  pstring - fully-featured string library for C

    Copyright 2025-2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <pstring/core.h>
#include <pstring/search.h>
#include <pstring/transform.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

int pstrcat(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrlen(src) > 0) {
        if (pstrreserve(dst, pstrlen(src)))
            return PSTRING_ENOMEM;

        memcpy(pstrend(dst), pstrbuf(src), pstrlen(src));
        pstr__setlen(dst, pstrlen(dst) + pstrlen(src));
    }

    return PSTRING_OK;
}

int pstrcats(pstring_t *dst, const char *src, size_t length) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (length == 0)
        length = strlen(src);

    if (length > 0) {
        if (pstrreserve(dst, length))
            return PSTRING_ENOMEM;

        memcpy(pstrend(dst), src, length);
        pstr__setlen(dst, pstrlen(dst) + length);
    }

    return PSTRING_OK;
}

int pstrcatc(pstring_t *dst, char chr) {
    if (!dst)
        return PSTRTHROW_EINVAL;

    if (pstrreserve(dst, 1))
        return PSTRING_ENOMEM;

    *pstrend(dst) = chr;
    pstr__setlen(dst, pstrlen(dst) + 1);
    return PSTRING_OK;
}

int pstrrcat(pstring_t *dst, const pstring_t *src) {
    if (!dst || !src)
        return PSTRTHROW_EINVAL;

    if (pstrlen(src) > 0) {
        if (pstrreserve(dst, pstrlen(src)))
            return PSTRING_ENOMEM;

        memmove(pstrslot(dst, pstrlen(src)), pstrbuf(dst), pstrlen(dst));
        memcpy(pstrbuf(dst), pstrbuf(src), pstrlen(src));
        pstr__setlen(dst, pstrlen(dst) + pstrlen(src));
    }

    return PSTRING_OK;
}

int pstrrcats(pstring_t *dst, const char *src, size_t length) {
    pstring_t tmp;
    pstrwrap(&tmp, (char *)src, length, length);
    return pstrrcat(dst, &tmp);
}

static int pstr__move(pstring_t *dst, size_t at, size_t count) {
    if (pstrreserve(dst, count))
        return PSTRTHROW_ENOMEM;

    size_t dlen = pstrlen(dst);

    if (at < dlen)
        memmove(pstrslot(dst, at + count), pstrslot(dst, at), dlen - at);

    pstr__setlen(dst, dlen + count);
    return PSTRING_OK;
}

int pstrinsert(pstring_t *dst, size_t at, pstring_t *src) {
    if (!dst || !src || at > pstrlen(dst))
        return PSTRTHROW_EINVAL;

    if (pstrlen(src) == 0)
        return PSTRING_OK;

    if (pstr__move(dst, at, pstrlen(src)))
        return PSTRTHROW_ENOMEM;

    memcpy(pstrslot(dst, at), pstrbuf(src), pstrlen(src));
    return PSTRING_OK;
}

int pstrinsertc(pstring_t *dst, size_t at, size_t count, char chr) {
    if (!dst || count == 0 || at > pstrlen(dst))
        return PSTRTHROW_EINVAL;

    if (pstr__move(dst, at, count))
        return PSTRTHROW_ENOMEM;

    memset(pstrslot(dst, at), chr, count);
    return PSTRING_OK;
}

int pstrinserts(pstring_t *dst, size_t at, const char *src, size_t length) {
    pstring_t tmp;
    pstrwrap(&tmp, (char *)src, length, length);
    return pstrinsert(dst, at, &tmp);
}

int pstrremove(pstring_t *str, size_t from, size_t to) {
    if (!str || from >= to)
        return PSTRTHROW_EINVAL;

    size_t len = pstrlen(str);
    if (from >= len || to > len)
        return PSTRTHROW_EINVAL;

    if (to < len)
        memmove(pstrslot(str, from), pstrslot(str, to), len - to);
    pstr__setlen(str, len - (to - from));
    return PSTRING_OK;
}

int pstrjoin(pstring_t *dst, const pstring_t *srcs, size_t count) {
    if (!dst || !srcs)
        return PSTRTHROW_EINVAL;

    size_t req = 0;
    for (size_t i = 0; i < count; i++)
        req += pstrlen(&srcs[i]);

    if (req > 0) {
        if (pstrreserve(dst, req))
            return PSTRING_ENOMEM;

        req = pstrlen(dst);
        for (size_t i = 0; i < count; i++) {
            memcpy(&pstrbuf(dst)[req], pstrbuf(&srcs[i]), pstrlen(&srcs[i]));
            req += pstrlen(&srcs[i]);
        }

        pstr__setlen(dst, req);
    }
    return PSTRING_OK;
}

int pstrrepl(
    pstring_t *str, const pstring_t *src, const pstring_t *dst, size_t max
) {
    if (!str || !src || !dst)
        return PSTRTHROW_EINVAL;

    if (max == 0)
        max = SIZE_MAX;

    size_t slen = pstrlen(src);
    size_t dlen = pstrlen(dst);

    pstring_t search;
    pstrslice(&search, str, 0, pstrlen(str));
    size_t length = pstrlen(str);
    ptrdiff_t diff = (ptrdiff_t)dlen - (ptrdiff_t)slen;
    char *match;

    while (max-- > 0) {
        if (!(match = pstrstr(&search, src)))
            break;

        if (diff > 0) {
            size_t offset = match - pstrbuf(str);

            if (pstrreserve(str, (size_t)diff))
                return PSTRING_ENOMEM;

            /* the buffer may have moved under us */
            match = pstrbuf(str) + offset;
        }

        /* source and destination overlap whenever the lengths differ */
        memmove(&match[dlen], match + slen, pstrend(str) - match - slen);

        if (dlen > 0)
            memcpy(match, pstrbuf(dst), dlen);

        length += diff;
        pstr__setlen(str, length);
        pstrrange(&search, NULL, &match[dlen], pstrend(str));
    }

    return PSTRING_OK;
}

int pstrrepls(pstring_t *str, const char *src, const char *dst, size_t max) {
    if (!src || !dst)
        return PSTRTHROW_EINVAL;

    pstring_t _src, _dst;
    pstrwrap(&_src, (char *)src, 0, 0);
    pstrwrap(&_dst, (char *)dst, 0, 0);
    return pstrrepl(str, &_src, &_dst, max);
}

int pstrreplc(pstring_t *str, char src, char dst, size_t max) {
    if (!str || src == dst)
        return PSTRTHROW_EINVAL;

    if (max == 0)
        max = SIZE_MAX;

    pstring_t search;
    char *match = pstrbuf(str);

    pstrrange(&search, NULL, match, pstrend(str));

    while (max-- > 0 && (match = pstrchr(&search, src))) {
        *match = dst;
        pstrrange(&search, NULL, match + 1, pstrend(str));
    }

    return PSTRING_OK;
}

int pstrlstrip(pstring_t *str, const char *chars) {
    if (!str)
        return PSTRTHROW_EINVAL;

    if (!chars)
        chars = " \t\r\n\v\f";

    char *left = pstrcpbrk(str, chars);
    if (left == NULL)
        return PSTRING_OK;

    return pstrcut(str, left - pstrbuf(str), pstrlen(str));
}

int pstrrstrip(pstring_t *str, const char *chars) {
    if (!str)
        return PSTRTHROW_EINVAL;

    if (!chars)
        chars = " \t\r\n\v\f";

    char *right = pstrrcpbrk(str, chars);
    if (right == NULL)
        return PSTRING_OK;

    return pstrcut(str, 0, right - pstrbuf(str) + 1);
}

int pstrstrip(pstring_t *str, const char *chars) {
    if (!str)
        return PSTRTHROW_EINVAL;

    if (!chars)
        chars = " \t\r\n\v\f";

    char *left = pstrcpbrk(str, chars);
    if (left == NULL)
        left = pstrbuf(str);

    pstring_t tmp;
    pstrrange(&tmp, NULL, left, pstrend(str));

    char *right = pstrrcpbrk(&tmp, chars);
    if (right == NULL)
        right = pstrend(str);

    return pstrcut(str, left - pstrbuf(str), (right + 1) - pstrbuf(str));
}

static int count_indent(const pstring_t *str, int max, int tab, int *out) {
    const char *chr;
    int length = 0;
    int count = 0;

    for (chr = pstrbuf(str); count < max && chr < pstrend(str); chr++) {
        if (*chr == ' ' || *chr == '\t') {
            count += *chr == '\t' ? tab : 1;
            length++;
        } else if (*chr != '\r' && *chr != '\v' && *chr != '\f') {
            break;
        }
    }

    if (out)
        *out = count;
    return length;
}

int pstrdedent(pstring_t *str, int count, int tab) {
    if (!str)
        return PSTRTHROW_EINVAL;

    if (count <= 0)
        count = INT_MAX;
    if (tab <= 0)
        tab = 4;

    pstring_t search;
    char *prev = pstrbuf(str);
    char *end = pstrend(str);
    char *match = prev;
    char *out = prev;

    pstrrange(&search, NULL, prev, pstrend(str));

    while (match < end) {
        match = pstrchr(&search, '\n');
        if (!match)
            match = end;

        pstrrange(&search, NULL, prev, match);

        int length = count_indent(&search, count, tab, NULL);
        memmove(out, &prev[length], match - prev - length + 1);
        out += match - prev - length + 1;

        prev = match + 1;
        pstrrange(&search, NULL, prev, pstrend(str));
    }

    pstr__setlen(str, out - pstrbuf(str) - 1);
    return PSTRING_OK;
}

int pstrindent(pstring_t *str, int count, int tab) {
    if (!str)
        return PSTRTHROW_EINVAL;

    if (count < 0)
        count = 0;
    if (tab <= 0)
        tab = 4;

    size_t prev = 0;
    pstring_t search;
    int indent, min = -1;

    while (prev < pstrlen(str)) {
        pstrrange(&search, NULL, &pstrbuf(str)[prev], pstrend(str));

        char *found = pstrchr(&search, '\n');
        size_t match = found ? (size_t)(found - pstrbuf(str)) : pstrlen(str);

        pstrrange(&search, NULL, &pstrbuf(str)[prev], &pstrbuf(str)[match]);

        if (count <= 0) {
            count_indent(&search, INT_MAX, tab, &indent);
            if (min == -1 || min > indent)
                min = indent;
        } else if (pstrinsertc(str, prev, count, ' '))
            return PSTRING_ENOMEM;

        prev = match + count + 1;
    }

    return min == -1 ? 0 : min;
}