# Core functions for interacting with pstrings

The `pstring_t` structure describes strings using 4 variables:
- string's character buffer (`pstrbuf`),
- the number of characters inside the buffer (`pstrlen`),
- buffer's size/capacity (`pstrcap`),
- allocator which owns the buffer's memory (`pstrallocator`).

Strings initialized with `{0}` or created using `pstrnew`, `pstralloc`,
`pstrdup` and others, own their buffers and can be resized and expanded,
but require calling `pstrfree` to free memory resources.

Strings can also be initialized as slices using `pstrwrap`, `pstrslice` and
`pstrrange`. Since slices represent chunks of other strings, they don't
need to be freed, but their buffer needs to be valid during their usage.

Shorter strings can be stored directly in the `pstring_t` using SSO,
provided they have been allocated using the default allocator. This mechanism
is not noticable most of the time, but can cause obscure bugs if misused.

- [`pstring_error`](#pstring_error) — Error codes are negated versions of POSIX ones.
- [`pstrcatch`](#pstrcatch) — This function set's up the exception handler for the `<pf_exception.h>` exception handling interface.
- [`PSTRTHROW`](#pstrthrow) — These macros emit an exception with the given error code and message.
- [`pstrthrow`](#pstrthrow-2) — Emits an exception with given message.
- [`pstrthrowf`](#pstrthrowf) — Emits an exception with given formatting.
- [`pstrvthrowf`](#pstrvthrowf) — Emits an exception with given formatting.
- [`pstrrethrow`](#pstrrethrow) — Emits the exception `e` for the next catcher.
- [`pstrdetect`](#pstrdetect) — When `PSTRING_DETECT` is defined, this function detects the SIMD capabilities of the CPU at runtime.
- [`pstrbuf`](#pstrbuf) — Returns the character buffer of `str`.
- [`pstrlen`](#pstrlen) — Returns the length, number of bytes, of `str`.
- [`pstrcap`](#pstrcap) — Returns the number of bytes allocated by `str`.
- [`pstrallocator`](#pstrallocator) — Returns the allocator used by `str` or `NULL` if it's a slice.
- [`pstrsso`](#pstrsso) — Checks if `str` is stored on the stack.
- [`pstrowned`](#pstrowned) — Checks if `str` can be resized (not a slice).
- [`pstrend`](#pstrend) — Returns the address pointing to the end of the character buffer, which, if `str` is owned, points to a `\0` character.
- [`pstrget`](#pstrget) — Returns the character at index `i` or `'\0'` if out of bounds
- [`pstrslot`](#pstrslot) — Returns the pointer to the slot `i` or `NULL` if out of bounds
- [`pstrnew`](#pstrnew) — Initializes `out` by copying the contents of `str`.
- [`pstrdup`](#pstrdup) — Copies the contents of `str` into `out`, like `pstrnew`.
- [`pstralloc`](#pstralloc) — Initializes `out` and reserves `capacity` bytes.
- [`pstrfree`](#pstrfree) — Frees all resources used by `str`, if it is owned.
- [`pstrarray_free`](#pstrarray_free) — Frees all strings in array `a`, if they are owned.
- [`pstrdump`](#pstrdump) — Copies the contents of `str` to `buffer` as a null-terminated string.
- [`pstrterm`](#pstrterm) — Ensures that `str` is null-terminated by copying it's content to a new buffer and appending '\0', if it's not currently null-terminated.
- [`pstrterms`](#pstrterms) — Ensures that `str` is null-terminated by copying it's content to the given buffer and appending '\0', if it's not currently null-terminated.
- [`pstristerm`](#pstristerm) — Checks if `str` is null-terminated.
- [`pstrunwrap`](#pstrunwrap) — Returns the character buffer of `str`.
- [`pstrwrap`](#pstrwrap) — Initializes `out` as a slice, using the `buffer` for storage.
- [`pstrwrapb`](#pstrwrapb) — Initializes `out` as a slice, using the `buffer` for storage.
- [`pstrslice`](#pstrslice) — Initializes `out` as a slice of bytes from `str`, starting at `from` (inclusive) and ending at `to` (exclusive).
- [`pstrcut`](#pstrcut) — Removes characters around the specified slice of bytes from `str`, starting at `from` (inclusive) and ending at `to` (exclusive).
- [`pstrrange`](#pstrrange) — Initializes `out` as a range of bytes from `str`, starting at `from` (inclusive) and ending at `to` (exclusive).
- [`PSTRWRAP`](#pstrwrap-2) — Initializes a `pstring_t` as a slice of `str`, which is assumed to be a null-terminated statically allocated string/array.
- [`PSTR`](#pstr) — Returns a reference to `pstring_t` that is initialized as a slice of `str`, which is assumed to be a null-terminated statically allocated string/array.
- [`pstrreserve`](#pstrreserve) — Reserves space to fit additional `count` items in `str`
- [`pstrgrow`](#pstrgrow) — Extends `str`'s buffer by at least `count` bytes.
- [`pstrshrink`](#pstrshrink) — Shrinks `str`'s buffer to use as little space as possible.
- [`pstrclear`](#pstrclear) — Removes all characters from `str`, setting it's length to 0.
- [`pstrequal`](#pstrequal) — Checks if `left` and `right` pstring are equal.
- [`pstrequals`](#pstrequals) — Checks if `left` pstring and `right` string are equal.
- [`pstrcmp`](#pstrcmp) — Compares `left` and `right` lexicographically, returning: - a positive number if `left` should appear before `right`.
- [`pstrcpy`](#pstrcpy) — Copies the contents of `src` into `dst`
- [`pstrhash`](#pstrhash) — Returns a non-unique integer value representing the contents of `str`.
- [`pstrdistance`](#pstrdistance) — Returns Damerau–Levenshtein distance between `left` and `right`.
- [`pstrshift`](#pstrshift) — This function slices `str` by moving the starting index by `right` and the ending index by `left` number of characters.
- [`pstrprefix`](#pstrprefix) — Checks if `str` starts with a `prefix`.
- [`pstrsuffix`](#pstrsuffix) — Checks if `str` ends with a `suffix`.
- [`pstrenv`](#pstrenv) — Gets the value of the environment variable `name` and slices it as `out`.
- [`pstr__setlen`](#pstr__setlen) — Naively sets the length of `str` to `length`

<a id="pstring_error"></a>

## `pstring_error`

```c
enum pstring_error {
    PSTRING_OK = 0,
    PSTRING_ENOENT = -2,
    PSTRING_EINTR = -4,
    PSTRING_EIO = -5,
    PSTRING_ENOMEM = -12,
    PSTRING_EEXIST = -17,
    PSTRING_EINVAL = -22,
    PSTRING_EDOM = -33,
    PSTRING_ERANGE = -34,
    PSTRING_ENOSYS = -38,
    PSTRING_ENODATA = -61,
    PSTRING_EENCODE = -192,
    PSTRING_EDECODE = -193,
    PSTRING_EOBJECT = -194,
};
```

Error codes are negated versions of POSIX ones.

<a id="pstrcatch"></a>

## `pstrcatch`

```c
pf_exception_t *pstrcatch(int error);
```

This function set's up the exception handler for the `<pf_exception.h>`
exception handling interface. The first call will return `NULL`, after
which exception-throwing code can be executed.

The second return value will represent the emitted exception.

> The global exception stack is thread local.

<a id="pstrthrow"></a>

## `PSTRTHROW`

```c
#define PSTRTHROW(code, msg)
#define PSTRTHROWF(code, fmt, ...)
#define PSTRVTHROWF(code, fmt, args)
#define PSTRTHROW_NULL(code)
#define PSTRTHROW_ENOENT PSTRTHROW(PSTRING_ENOENT, NULL)
#define PSTRTHROW_EINTR PSTRTHROW(PSTRING_EINTR, NULL)
#define PSTRTHROW_EIO PSTRTHROW(PSTRING_EIO, NULL)
#define PSTRTHROW_ENOMEM PSTRTHROW(PSTRING_ENOMEM, NULL)
#define PSTRTHROW_EEXIST PSTRTHROW(PSTRING_EEXIST, NULL)
#define PSTRTHROW_EINVAL PSTRTHROW(PSTRING_EINVAL, NULL)
#define PSTRTHROW_EDOM PSTRTHROW(PSTRING_EDOM, NULL)
#define PSTRTHROW_ERANGE PSTRTHROW(PSTRING_ERANGE, NULL)
#define PSTRTHROW_ENOSYS PSTRTHROW(PSTRING_ENOSYS, NULL)
#define PSTRTHROW_ENODATA PSTRTHROW(PSTRING_ENODATA, NULL)
#define PSTRTHROW_EENCODE PSTRTHROW(PSTRING_EENCODE, NULL)
#define PSTRTHROW_EDECODE PSTRTHROW(PSTRING_EDECODE, NULL)
#define PSTRTHROW_EOBJECT PSTRTHROW(PSTRING_EOBJECT, NULL)
```

These macros emit an exception with the given error code and message.

<a id="pstrthrow-2"></a>

## `pstrthrow`

```c
PSTR_NORETURN   int pstrthrow(
    int code, const char *func, const char *msg
);
```

Emits an exception with given message.

<a id="pstrthrowf"></a>

## `pstrthrowf`

```c
PSTR_NORETURN   int pstrthrowf(
    int code, const char *func, const char *fmt, ...
);
```

Emits an exception with given formatting.

<a id="pstrvthrowf"></a>

## `pstrvthrowf`

```c
PSTR_NORETURN   int pstrvthrowf(
    int code, const char *func, const char *fmt, va_list args
);
```

Emits an exception with given formatting.

<a id="pstrrethrow"></a>

## `pstrrethrow`

```c
PSTR_NORETURN   int pstrrethrow(pf_exception_t *e);
```

Emits the exception `e` for the next catcher.

<a id="pstrdetect"></a>

## `pstrdetect`

```c
void pstrdetect(void);
```

When `PSTRING_DETECT` is defined, this function detects the
SIMD capabilities of the CPU at runtime. Otherwise, the function
immediately exits, while the SIMD detection occurs at compile-time.

<a id="pstrbuf"></a>

## `pstrbuf`

```c
PSTR_INLINE char *pstrbuf(const pstring_t *str);
```

Returns the character buffer of `str`. If `str` is resized after calling
this function, the returned pointer should be considered invalid.

<a id="pstrlen"></a>

## `pstrlen`

```c
PSTR_INLINE size_t pstrlen(const pstring_t *str);
```

Returns the length, number of bytes, of `str`.

<a id="pstrcap"></a>

## `pstrcap`

```c
PSTR_INLINE size_t pstrcap(const pstring_t *str);
```

Returns the number of bytes allocated by `str`.

<a id="pstrallocator"></a>

## `pstrallocator`

```c
PSTR_INLINE allocator_t *pstrallocator(const pstring_t *str);
```

Returns the allocator used by `str` or `NULL` if it's a slice.

<a id="pstrsso"></a>

## `pstrsso`

```c
PSTR_INLINE int pstrsso(const pstring_t *str);
```

Checks if `str` is stored on the stack.

<a id="pstrowned"></a>

## `pstrowned`

```c
PSTR_INLINE int pstrowned(const pstring_t *str);
```

Checks if `str` can be resized (not a slice).

<a id="pstrend"></a>

## `pstrend`

```c
PSTR_INLINE char *pstrend(const pstring_t *str);
```

Returns the address pointing to the end of the character buffer,
which, if `str` is owned, points to a `\0` character.

<a id="pstrget"></a>

## `pstrget`

```c
PSTR_INLINE char pstrget(const pstring_t *str, size_t i);
```

Returns the character at index `i` or `'\0'` if out of bounds

<a id="pstrslot"></a>

## `pstrslot`

```c
PSTR_INLINE char *pstrslot(const pstring_t *str, size_t i);
```

Returns the pointer to the slot `i` or `NULL` if out of bounds

<a id="pstrnew"></a>

## `pstrnew`

```c
int pstrnew(
    pstring_t *out, const char *str, size_t len, allocator_t *alloc
);
```

Initializes `out` by copying the contents of `str`.
If `len` is `0` and `str` is not empty, `strlen` is called.
If `alloc` is `NULL`, the standard allocator is used.

**Errors**

EINVAL, ENOMEM.

<a id="pstrdup"></a>

## `pstrdup`

```c
int pstrdup(
    pstring_t *out, const pstring_t *str, allocator_t *allocator
);
```

Copies the contents of `str` into `out`, like `pstrnew`.
You can use this function to turn slices into owned pstrings.

**Errors**

EINVAL, ENOMEM.

<a id="pstralloc"></a>

## `pstralloc`

```c
int pstralloc(pstring_t *out, size_t capacity, allocator_t *alloc);
```

Initializes `out` and reserves `capacity` bytes.

**Errors**

EINVAL, ENOMEM.

<a id="pstrfree"></a>

## `pstrfree`

```c
void pstrfree(pstring_t *str);
```

Frees all resources used by `str`, if it is owned.

<a id="pstrarray_free"></a>

## `pstrarray_free`

```c
void pstrarray_free(pstrarray_t *array);
```

Frees all strings in array `a`, if they are owned.

<a id="pstrdump"></a>

## `pstrdump`

```c
int pstrdump(const pstring_t *str, char *buffer, size_t size);
```

Copies the contents of `str` to `buffer` as a null-terminated string.

**Errors**

EINVAL, ENOMEM.

<a id="pstrterm"></a>

## `pstrterm`

```c
int pstrterm(pstring_t *str, allocator_t *allocator);
```

Ensures that `str` is null-terminated by copying it's content to a new
buffer and appending '\0', if it's not currently null-terminated.

**Errors**

EINVAL, ENOMEM.

<a id="pstrterms"></a>

## `pstrterms`

```c
char *pstrterms(pstring_t *str, char *buffer, size_t size);
```

Ensures that `str` is null-terminated by copying it's content to the given
buffer and appending '\0', if it's not currently null-terminated.
Returned string is either `buffer` or `pstrbuf(str)`.

<a id="pstristerm"></a>

## `pstristerm`

```c
PSTR_INLINE int pstristerm(pstring_t *str);
```

Checks if `str` is null-terminated.

<a id="pstrunwrap"></a>

## `pstrunwrap`

```c
char *pstrunwrap(const pstring_t *str, allocator_t *allocator);
```

Returns the character buffer of `str`. If `str` is using SSO,
the contents of `str` will be copied to a brand new buffer.

<a id="pstrwrap"></a>

## `pstrwrap`

```c
int pstrwrap(
    pstring_t *out, char *buffer, size_t length, size_t capacity
);
```

Initializes `out` as a slice, using the `buffer` for storage.
If `length` is `0`, `strlen` is used to calculate it's length.
If `length` is `0` and capacity is not, `strnlen` is used instead.
If `capacity` is `0` it is set to the computed length.

**Errors**

EINVAL.

<a id="pstrwrapb"></a>

## `pstrwrapb`

```c
int pstrwrapb(
    pstring_t *out, char *buffer, size_t length, size_t capacity
);
```

Initializes `out` as a slice, using the `buffer` for storage.
No `strlen` is performed by this variant.

**Errors**

EINVAL.

<a id="pstrslice"></a>

## `pstrslice`

```c
int pstrslice(
    pstring_t *out, const pstring_t *str, size_t from, size_t to
);
```

Initializes `out` as a slice of bytes from `str`, starting at `from`
(inclusive) and ending at `to` (exclusive). Both indices are set to
the length of `str` if they are larger. If `to` is smaller than
`from`, a zero-length slice at `to` is taken.

**Errors**

EINVAL.

<a id="pstrcut"></a>

## `pstrcut`

```c
int pstrcut(pstring_t *str, size_t from, size_t to);
```

Removes characters around the specified slice of bytes from `str`,
starting at `from` (inclusive) and ending at `to` (exclusive).
Both indices are set to the length of `str` if they are larger.
If `to` is smaller than `from`, the length of `str` is set to zero.

> If `str` is a slice, it is repositioned instead.

**Errors**

EINVAL.

<a id="pstrrange"></a>

## `pstrrange`

```c
int pstrrange(
    pstring_t *out, const pstring_t *str, const char *from, const char *to
);
```

Initializes `out` as a range of bytes from `str`, starting
at `from` (inclusive) and ending at `to` (exclusive).
If `from` is `NULL` or invalid, it's set to the start of the pstring.
If `to` is `NULL` or invalid, it's set to the end of the pstring.

**Errors**

EINVAL.

<a id="pstrwrap-2"></a>

## `PSTRWRAP`

```c
#define PSTRWRAP(str)
```

Initializes a `pstring_t` as a slice of `str`, which is assumed
to be a null-terminated statically allocated string/array.

<a id="pstr"></a>

## `PSTR`

```c
#define PSTR(str)
```

Returns a reference to `pstring_t` that is initialized as a slice of `str`,
which is assumed to be a null-terminated statically allocated string/array.

<a id="pstrreserve"></a>

## `pstrreserve`

```c
int pstrreserve(pstring_t *str, size_t count);
```

Reserves space to fit additional `count` items in `str`

**Errors**

EINVAL, ENOMEM.

<a id="pstrgrow"></a>

## `pstrgrow`

```c
int pstrgrow(pstring_t *str, size_t count);
```

Extends `str`'s buffer by at least `count` bytes.

**Errors**

EINVAL, ENOMEM.

<a id="pstrshrink"></a>

## `pstrshrink`

```c
int pstrshrink(pstring_t *str);
```

Shrinks `str`'s buffer to use as little space as possible.

**Errors**

EINVAL, ENOMEM.

<a id="pstrclear"></a>

## `pstrclear`

```c
#define pstrclear(str)
```

Removes all characters from `str`, setting it's length to 0.

<a id="pstrequal"></a>

## `pstrequal`

```c
int pstrequal(const pstring_t *left, const pstring_t *right);
int pstrequalb(
    const pstring_t *left, const char *right, size_t length
);
```

Checks if `left` and `right` pstring are equal.

<a id="pstrequals"></a>

## `pstrequals`

```c
int pstrequals(
    const pstring_t *left, const char *right, size_t length
);
```

Checks if `left` pstring and `right` string are equal.
If `length` is zero, `right` is treated as a null-terminated string.

<a id="pstrcmp"></a>

## `pstrcmp`

```c
int pstrcmp(const pstring_t *left, const pstring_t *right);
int pstrcmps(const pstring_t *left, const char *right, size_t length);
```

Compares `left` and `right` lexicographically, returning:
- a positive number if `left` should appear before `right`.
- a negative number if `left` should appear after `right`.
- `0` if they are equal.

<a id="pstrcpy"></a>

## `pstrcpy`

```c
int pstrcpy(pstring_t *dst, const pstring_t *src);
```

Copies the contents of `src` into `dst`

**Errors**

EINVAL, ENOMEM.

<a id="pstrhash"></a>

## `pstrhash`

```c
size_t pstrhash(const pstring_t *str);
```

Returns a non-unique integer value representing the contents of `str`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrdistance"></a>

## `pstrdistance`

```c
int pstrdistance(const pstring_t *left, const pstring_t *right);
```

Returns Damerau–Levenshtein distance between `left` and `right`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrshift"></a>

## `pstrshift`

```c
PSTR_INLINE int pstrshift(pstring_t *str, size_t left, size_t right);
PSTR_INLINE int pstrlshift(pstring_t *str, size_t count);
PSTR_INLINE int pstrrshift(pstring_t *str, size_t count);
```

This function slices `str` by moving the starting index by `right` and
the ending index by `left` number of characters. (`str` must be a slice)

**Errors**

EINVAL.

<a id="pstrprefix"></a>

## `pstrprefix`

```c
int pstrprefix(
    const pstring_t *str, const char *prefix, size_t length
);
```

Checks if `str` starts with a `prefix`.

<a id="pstrsuffix"></a>

## `pstrsuffix`

```c
int pstrsuffix(
    const pstring_t *str, const char *suffix, size_t length
);
```

Checks if `str` ends with a `suffix`.

<a id="pstrenv"></a>

## `pstrenv`

```c
int pstrenv(pstring_t *out, const pstring_t *name);
```

Gets the value of the environment variable `name` and slices it as `out`.
Object `name` can be a slice, i.e. null termination is not required.

**Errors**

EINVAL, ENOENT, ENOMEM.

<a id="pstr__setlen"></a>

## `pstr__setlen`

```c
PSTR_INLINE void pstr__setlen(pstring_t *str, size_t length);
```

Naively sets the length of `str` to `length`

