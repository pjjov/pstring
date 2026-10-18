# wordexp

- [`pstrexpand_trim_t`](#pstrexpand_trim_t) — Payload passed as `src` to a callback invoked with kind == `PSTREXPAND_TRIM`, used by the `${var#pattern}` family of parameter-expansion operators.
- [`pstrexpand_fn`](#pstrexpand_fn) — Callback used for the `wordexp` shell expansion.
- [`pstrexpand`](#pstrexpand) — Performs word expansion using the default callback.
- [`pstrexpand_with`](#pstrexpand_with) — Performs word expansion using the provided callback.
- [`pstrexpand_default_cb`](#pstrexpand_default_cb) — Default callback used for `wordexp` shell expansion.

<a id="pstrexpand_trim_t"></a>

## `pstrexpand_trim_t`

```c
typedef struct pstrexpand_trim_t {
    const pstring_t *value;
    pstring_t *pattern;
    int suffix;
    int greedy;
} pstrexpand_trim_t;
```

Payload passed as `src` to a callback invoked with kind ==
`PSTREXPAND_TRIM`, used by the `${var#pattern}` family of
parameter-expansion operators. `value` holds the variable's current
value; `pattern` holds the (unexpanded) glob pattern text; `suffix`
distinguishes `%`/`%%` (trim from the end) from `#`/`##` (trim from
the start); `greedy` distinguishes the doubled `##`/`%%` form
(longest match) from the single `#`/`%` form (shortest match).

<a id="pstrexpand_fn"></a>

## `pstrexpand_fn`

```c
typedef int(pstrexpand_fn)(
    void *dst, void *src, int flags, int kind, void *user
);
```

Callback used for the `wordexp` shell expansion.

<a id="pstrexpand"></a>

## `pstrexpand`

```c
int pstrexpand(
    pstrarray_t *dst, pstring_t *src, int flags, pstrexpand_fn *cb
);
```

Performs word expansion using the default callback.

<a id="pstrexpand_with"></a>

## `pstrexpand_with`

```c
int pstrexpand_with(
    pstrarray_t *dst, pstring_t *src, int flags, pstrexpand_fn *cb, void *user
);
```

Performs word expansion using the provided callback.

<a id="pstrexpand_default_cb"></a>

## `pstrexpand_default_cb`

```c
int pstrexpand_default_cb(
    void *dst, void *src, int flags, int kind, void *user
);
```

Default callback used for `wordexp` shell expansion.

