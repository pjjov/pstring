# Streaming and serialization functions for pstrings.

The `pf_stream_t` object can be used as a generic interface for interacting
with byte streams. Unlike standard library `FILE`, `pf_stream_t` objects
can use user-defined stream implementations such as network sockets,
in-memory buffers, encoders and serializers.

This header also provides powerful serialization and deserialization
mechanisms through the `pstrmodel` and associated structures.

- [`pstrreadall`](#pstrreadall) — Concatenates the contents of the file onto the end of `out`.
- [`pstrwriteall`](#pstrwriteall) — Writes the entire string `out` to the file at `path`.
- [`pstrio_printf`](#pstrio_printf) — Concatenates string formated by standard library functions to `dst`.
- [`pstrio_vprintf`](#pstrio_vprintf) — Concatenates string formated by standard library functions to `dst`.
- [`pf_stream_pstring`](#pf_stream_pstring) — Initializes a stream that will read and write to the buffer of `str`, expanding it if needed.
- [`pf_stream_putp`](#pf_stream_putp) — Writes a `pstring_t` to `stream`.

<a id="pstrreadall"></a>

## `pstrreadall`

```c
int pstrreadall(pstring_t *out, const char *path);
```

Concatenates the contents of the file onto the end of `out`.

**Errors**

EINVAL, ENOMEM, EIO.

<a id="pstrwriteall"></a>

## `pstrwriteall`

```c
int pstrwriteall(const pstring_t *str, const char *path);
```

Writes the entire string `out` to the file at `path`.

**Errors**

EINVAL, EIO.

<a id="pstrio_printf"></a>

## `pstrio_printf`

```c
int pstrio_printf(pstring_t *dst, const char *fmt, ...);
```

Concatenates string formated by standard library functions to `dst`.

**Errors**

EINVAL, ENOMEM, EIO.

<a id="pstrio_vprintf"></a>

## `pstrio_vprintf`

```c
int pstrio_vprintf(pstring_t *dst, const char *fmt, va_list args);
```

Concatenates string formated by standard library functions to `dst`.
Arguments are passed as a variable arguments list from `<stdarg.h>`.

**Errors**

EINVAL, ENOMEM, EIO.

<a id="pf_stream_pstring"></a>

## `pf_stream_pstring`

```c
int pf_stream_pstring(pf_stream_t *out, pstring_t *str);
```

Initializes a stream that will read and write to the buffer
of `str`, expanding it if needed. `str` will NOT be freed by
the stream when closing.

Stream cursor will be at the end of the string.

**Errors**

EINVAL, EIO.

<a id="pf_stream_putp"></a>

## `pf_stream_putp`

```c
int pf_stream_putp(pf_stream_t *stream, const pstring_t *str);
```

Writes a `pstring_t` to `stream`.

**Errors**

EINVAL, EIO.

