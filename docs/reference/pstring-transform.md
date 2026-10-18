# String transformation functions

This module provides function for inserting, removing and replacing
parts of the string.

- [`pstrcat`](#pstrcat) — Concatenates `src` onto the end of `dst`.
- [`pstrrcat`](#pstrrcat) — Concatenates `src` onto the start of `dst`.
- [`pstrinsert`](#pstrinsert) — Inserts characters from `src` into `dst` at index `at`.
- [`pstrinsertc`](#pstrinsertc) — Inserts `chr` character `count` times into `dst` at index `at`.
- [`pstrremove`](#pstrremove) — Removes characters from `str` in the specified range.
- [`pstrjoin`](#pstrjoin) — Concatenates `count` pstrings from `srcs` onto `dst`.
- [`pstrrepl`](#pstrrepl) — Replaces at most `max` instances of substring `src` with `dst`.
- [`pstrstrip`](#pstrstrip) — Removes leading and trailing characters from `str` that are specified in `chars`.
- [`pstrdedent`](#pstrdedent) — Removes leading whitespace up to `count`, assuming that `\t` character is equivalent to `tab` blank characters (defaults to 4 instead).
- [`pstrindent`](#pstrindent) — Inserts leading whitespace up to `count`, assuming that `\t` character is equivalent to `tab` blank characters (defaults to 4 instead).

<a id="pstrcat"></a>

## `pstrcat`

```c
int pstrcat(pstring_t *dst, const pstring_t *src);
int pstrcats(pstring_t *dst, const char *src, size_t length);
int pstrcatc(pstring_t *dst, char chr);
PSTR_INLINE int pstrcatb(pstring_t *dst, const char *src, size_t length);
```

Concatenates `src` onto the end of `dst`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrrcat"></a>

## `pstrrcat`

```c
int pstrrcat(pstring_t *dst, const pstring_t *src);
int pstrrcats(pstring_t *dst, const char *src, size_t length);
PSTR_INLINE int pstrrcatc(pstring_t *dst, char chr);
```

Concatenates `src` onto the start of `dst`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrinsert"></a>

## `pstrinsert`

```c
int pstrinsert(pstring_t *dst, size_t at, pstring_t *src);
int pstrinserts(
    pstring_t *dst, size_t at, const char *src, size_t length
);
```

Inserts characters from `src` into `dst` at index `at`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrinsertc"></a>

## `pstrinsertc`

```c
int pstrinsertc(pstring_t *dst, size_t at, size_t count, char chr);
```

Inserts `chr` character `count` times into `dst` at index `at`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrremove"></a>

## `pstrremove`

```c
int pstrremove(pstring_t *str, size_t from, size_t to);
```

Removes characters from `str` in the specified range.

**Errors**

EINVAL, ENOMEM.

<a id="pstrjoin"></a>

## `pstrjoin`

```c
int pstrjoin(pstring_t *dst, const pstring_t *srcs, size_t count);
```

Concatenates `count` pstrings from `srcs` onto `dst`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrrepl"></a>

## `pstrrepl`

```c
int pstrrepl(
    pstring_t *str, const pstring_t *src, const pstring_t *dst, size_t max
);
int pstrrepls(
    pstring_t *str, const char *src, const char *dst, size_t max
);
int pstrreplc(pstring_t *str, char src, char dst, size_t max);
```

Replaces at most `max` instances of substring `src` with `dst`.
If `max` is zero, all instances of `src` will be replaced.

**Errors**

EINVAL, ENOMEM.

<a id="pstrstrip"></a>

## `pstrstrip`

```c
int pstrstrip(pstring_t *str, const char *chars);
int pstrlstrip(pstring_t *str, const char *chars);
int pstrrstrip(pstring_t *str, const char *chars);
```

Removes leading and trailing characters from `str` that are specified in
`chars`. If `str` is a slice, it will be resliced to omit them instead.

The `pstrlstrip` variant only removes leading, while `pstrrstrip` only
removes trailing characters from `str` that are specified in `chars`.

**Errors**

EINVAL.

<a id="pstrdedent"></a>

## `pstrdedent`

```c
int pstrdedent(pstring_t *str, int count, int tab);
```

Removes leading whitespace up to `count`, assuming that `\t` character
is equivalent to `tab` blank characters (defaults to 4 instead).
If `count` is zero or less, all whitespace is removed.

**Errors**

EINVAL.

<a id="pstrindent"></a>

## `pstrindent`

```c
int pstrindent(pstring_t *str, int count, int tab);
```

Inserts leading whitespace up to `count`, assuming that `\t` character
is equivalent to `tab` blank characters (defaults to 4 instead). If `count`
is zero or less, the minimum indentation already present is returned.

**Errors**

EINVAL.

