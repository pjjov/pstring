# Search functions for pstrings.

- [`pstrchr`](#pstrchr) — Searches for character `ch` from the start of `str`, returning it's address if found and `NULL` otherwise.
- [`pstrpbrk`](#pstrpbrk) — Searches for a character in `str` that is also found in `set`, returning it's address if found and `NULL` otherwise.
- [`pstrspn`](#pstrspn) — Returns the number of consecutive characters that appear at the start of `str` that are included in the `set`.
- [`pstrstr`](#pstrstr) — Searches for `sub` inside `str`, returning the address of the first character of the first match, or `NULL` if not found.
- [`pstrtok`](#pstrtok) — Tokenizes input string `src` into a sequence of tokens separated by a character inside `set`.
- [`pstrsplit`](#pstrsplit) — Tokenizes input string `src` into a sequence of tokens separated by a substring `sep`.

<a id="pstrchr"></a>

## `pstrchr`

```c
char *pstrchr(const pstring_t *str, int ch);
char *pstrrchr(const pstring_t *str, int ch);
```

Searches for character `ch` from the start of `str`,
returning it's address if found and `NULL` otherwise.

The `pstrrchr` variant searches from the end instead.

<a id="pstrpbrk"></a>

## `pstrpbrk`

```c
char *pstrpbrk(const pstring_t *str, const char *set);
char *pstrcpbrk(const pstring_t *str, const char *set);
char *pstrrpbrk(const pstring_t *str, const char *set);
char *pstrrcpbrk(const pstring_t *str, const char *set);
```

Searches for a character in `str` that is also found in `set`,
returning it's address if found and `NULL` otherwise.

The `pstrrpbrk` and `pstrrcpbrk` variants search from the end instead.

`pstrcpbrk` and `pstrrcpbrk` variants search for a character that is not
found in `set` returning it's address if found and `NULL` otherwise.

<a id="pstrspn"></a>

## `pstrspn`

```c
size_t pstrspn(const pstring_t *str, const char *set);
size_t pstrcspn(const pstring_t *str, const char *set);
size_t pstrrspn(const pstring_t *str, const char *set);
size_t pstrrcspn(const pstring_t *str, const char *set);
```

Returns the number of consecutive characters that appear
at the start of `str` that are included in the `set`.

The `pstrrspn` and `pstrrcspn` variants count from the end instead.

`pstrcspn` and `pstrrcspn` variants count the number of consecutive
characters that aren't included in `set`, from start and end respectively.

<a id="pstrstr"></a>

## `pstrstr`

```c
char *pstrstr(const pstring_t *str, const pstring_t *sub);
```

Searches for `sub` inside `str`, returning the address of the
first character of the first match, or `NULL` if not found.

<a id="pstrtok"></a>

## `pstrtok`

```c
int pstrtok(pstring_t *dst, const pstring_t *src, const char *set);
```

Tokenizes input string `src` into a sequence of tokens separated by
a character inside `set`. If not found, `PSTRING_ENOENT` is returned.

Tokens and state are stored in `dst`. To initialize `dst`,
call this function with `NULL` passed for `set`.

The next token is started at the first character not found in `set` and
ends in the first character that is found in `set`, or the end of `src`.

**Errors**

EINVAL, ENOENT.

<a id="pstrsplit"></a>

## `pstrsplit`

```c
int pstrsplit(
    pstring_t *dst, const pstring_t *src, const pstring_t *sep
);
int pstrsplits(
    pstring_t *dst, const pstring_t *src, const char *sep, size_t length
);
```

Tokenizes input string `src` into a sequence of tokens separated by
a substring `sep`. If not found, `PSTRING_ENOENT` is returned.

Tokens and state are stored in `dst`. To initialize `dst`,
call this function with `NULL` passed for `sep`.

If `sep` comes right after the end of `dst`, it is skipped before searching
for the next token. This behaviour can be suprising when using  different
separators between function calls.

**Errors**

EINVAL, ENOENT.

