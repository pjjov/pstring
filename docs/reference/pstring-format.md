# String formatting functions

This module provides time and string formatting functions.

Alongside standard formatting options, formatting functions offer extensions
for commonly used types that are missing from the standard library and
additional encoding support:

- `%P` - prints the passed `pstring_t *` argument.
- `%D` - prints calendar time using the passed format.
- `%?` - serializes a pointer to a type indicated by the passed `int` id.
- `%!` - encodes the following format option using the specified encoding.
- `%Ib`, `%Ub` - prints `int8_t` and `uint8_t` respectively.
- `%Iw`, `%Uw` - prints `int16_t` and `uint16_t` respectively.
- `%Id`, `%Ud` - prints `int32_t` and `uint32_t` respectively.
- `%Iq`, `%Uq` - prints `int64_t` and `uint64_t` respectively.
- `%Im`, `%Um` - prints `intmax_t` and `uintmax_t` respectively.
- `%Ip`, `%Up` - prints `intptr_t` and `uintptr_t` respectively.
- `%IP`, `%Us` - prints `ptrdiff_t` and `size_t` respectively.

```c
    pstring_t *str, *other;
    pstrfmt(str, "%P", other);

    int num;
    pstrfmt(str, "%?", PF_TYPE_INT, &num);

    struct tm date = { .tm_mday = 30 };
    pstrfmt(str, "%D", "%A %c", &date);

    pstrfmt(str, "%!html%P world!", PSTR("<Hello>"));

    pstrfmt(str, "Bye %!*%s!", "hex", "world");
```

- [`pstrfmt`](#pstrfmt) — Concatenates a string formatted according to `fmt`.
- [`pstrprintf`](#pstrprintf) — Prints a formatted string to standard output.
- [`pstrerrorf`](#pstrerrorf) — Prints a formatted string to standard error output.
- [`pstrfprintf`](#pstrfprintf) — Prints a formatted string to given character stream.
- [`pstrftime`](#pstrftime) — Concatenates a string formatted by `fmt` using date and time from `src`.

<a id="pstrfmt"></a>

## `pstrfmt`

```c
int pstrfmt(pstring_t *dst, const char *fmt, ...);
int pstrfmtv(pstring_t *dst, const char *fmt, va_list args);
```

Concatenates a string formatted according to `fmt`.

> This function supports formatting extensions.

**Errors**

EINVAL, ENOMEM.

<a id="pstrprintf"></a>

## `pstrprintf`

```c
int pstrprintf(const char *fmt, ...);
int pstrvprintf(const char *fmt, va_list args);
```

Prints a formatted string to standard output.

> This function supports formatting extensions.

**Errors**

EINVAL, <stdio.h> error codes.

<a id="pstrerrorf"></a>

## `pstrerrorf`

```c
int pstrerrorf(const char *fmt, ...);
int pstrverrorf(const char *fmt, va_list args);
```

Prints a formatted string to standard error output.

> This function supports formatting extensions.

**Errors**

EINVAL, <stdio.h> error codes.

<a id="pstrfprintf"></a>

## `pstrfprintf`

```c
int pstrfprintf(pf_stream_t *stream, const char *fmt, ...);
int pstrvfprintf(pf_stream_t *stream, const char *fmt, va_list args);
```

Prints a formatted string to given character stream.

> This function supports formatting extensions.

**Errors**

EINVAL, stream error codes.

<a id="pstrftime"></a>

## `pstrftime`

```c
int pstrftime(pstring_t *dst, const char *fmt, struct tm *src);
```

Concatenates a string formatted by `fmt` using date and time from `src`.

**Errors**

EINVAL, ENOMEM.

