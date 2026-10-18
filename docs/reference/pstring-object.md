# Dynamic object handling and serialization.

`pstrobj_t` represents a dynamically typed object which can be saved to and
loaded from various formats. Unlike `struct pstrmodel` which maps objects
directly to C structures, `pstrobj` maintains the whole object in memory.

- [`pstrobj_new`](#pstrobj_new) — Allocates a new object with NULL type.
- [`pstrobj_from_buffer`](#pstrobj_from_buffer) — Loads an object from `source` that is in specified `format`.
- [`pstrobj_from_stream`](#pstrobj_from_stream) — Loads an object in `format` by reading from `stream`.
- [`pstrobj_from_path`](#pstrobj_from_path) — Loads an object in `format` by reading from the file at `path`.
- [`pstrobj_to_buffer`](#pstrobj_to_buffer) — Saves an object to `source` in specified `format`.
- [`pstrobj_to_stream`](#pstrobj_to_stream) — Saves an object in `format` by writing to `stream`.
- [`pstrobj_free`](#pstrobj_free) — Frees object and it's children if it's detached.
- [`pstrobj_query`](#pstrobj_query) — Finds an object using the JSON Pointer format.
- [`pstrobj_set_*`](#pstrobj_set_) — Following functions set the object's value and type.
- [`pstrobj_expect_*`](#pstrobj_expect_) — Following functions return the object's value if `obj` has a matching type.
- [`pstrobj_query_*`](#pstrobj_query_) — Following functions query `obj` for a child and return the child's value if `obj` has a matching type.
- [`pstrobj_get_*`](#pstrobj_get_) — Following functions return the object's value or the default value `def`, depending on the type of `obj`, converting between types if necessary.
- [`pstrobj_copy_string`](#pstrobj_copy_string) — Copies the contents of `str` as the value of `obj`.
- [`pstrobj_wrap_string`](#pstrobj_wrap_string) — Wraps the contents of `str` as the value of `obj`.
- [`pstrobj_list_insert`](#pstrobj_list_insert) — Inserts `item` at index `i` inside `list`.
- [`pstrobj_list_remove`](#pstrobj_list_remove) — Removes item at index `i` in `list` and returns it.
- [`pstrobj_list_free`](#pstrobj_list_free) — Removes item at index `i` in `list` and frees it.
- [`pstrobj_dict_get`](#pstrobj_dict_get) — Finds an item in `dict` with specified `key`.
- [`pstrobj_dict_insert`](#pstrobj_dict_insert) — Inserts `item` into `dict` with a previously set key.
- [`pstrobj_dict_remove`](#pstrobj_dict_remove) — Removes item with specified key in `dict` and returns it.
- [`pstrobj_dict_free`](#pstrobj_dict_free) — Removes item with specified key in `dict` and frees it.

<a id="pstrobj_new"></a>

## `pstrobj_new`

```c
pstrobj_t *pstrobj_new(allocator_t *allocator);
```

Allocates a new object with NULL type.

<a id="pstrobj_from_buffer"></a>

## `pstrobj_from_buffer`

```c
pstrobj_t *pstrobj_from_buffer(
    const char *format, pstring_t *source, allocator_t *allocator
);
```

Loads an object from `source` that is in specified `format`.

<a id="pstrobj_from_stream"></a>

## `pstrobj_from_stream`

```c
pstrobj_t *pstrobj_from_stream(
    const char *format, pf_stream_t *stream, allocator_t *allocator
);
pstrobj_t *pstrobj_load_json(
    pf_stream_t *stream, allocator_t *allocator
);
pstrobj_t *pstrobj_load_xml(
    pf_stream_t *stream, allocator_t *allocator
);
```

Loads an object in `format` by reading from `stream`.

<a id="pstrobj_from_path"></a>

## `pstrobj_from_path`

```c
pstrobj_t *pstrobj_from_path(
    const char *format, const char *path, allocator_t *allocator
);
```

Loads an object in `format` by reading from the file at `path`.

<a id="pstrobj_to_buffer"></a>

## `pstrobj_to_buffer`

```c
int pstrobj_to_buffer(
    pstrobj_t *obj, const char *format, pstring_t *source
);
```

Saves an object to `source` in specified `format`.

<a id="pstrobj_to_stream"></a>

## `pstrobj_to_stream`

```c
int pstrobj_to_stream(
    pstrobj_t *obj, const char *format, pf_stream_t *stream
);
int pstrobj_save_json(pstrobj_t *obj, pf_stream_t *stream);
int pstrobj_save_xml(pstrobj_t *obj, pf_stream_t *stream);
```

Saves an object in `format` by writing to `stream`.

Since XML has no native list/dict distinction the way JSON does, the
mapping used is: the root object becomes a single `<root>` element;
a dict's members become child elements named after their keys; a
list's items become repeated `<item>` elements; a string/long/double/
bool becomes the element's text content; `null` becomes an empty
element. This is the same convention most JSON&lt;-&gt;XML converters
use, and is reversed exactly by `pstrobj_load_xml`.

<a id="pstrobj_free"></a>

## `pstrobj_free`

```c
void pstrobj_free(pstrobj_t *obj);
```

Frees object and it's children if it's detached.

<a id="pstrobj_query"></a>

## `pstrobj_query`

```c
pstrobj_t *pstrobj_query(pstrobj_t *obj, const char *query);
void *pstrobj_query_value(pstrobj_t *obj, const char *query);
```

Finds an object using the JSON Pointer format.

<a id="pstrobj_set_"></a>

## `pstrobj_set_*`

```c
int pstrobj_set_null(pstrobj_t *obj);
int pstrobj_set_bool(pstrobj_t *obj, char value);
int pstrobj_set_int(pstrobj_t *obj, int value);
int pstrobj_set_long(pstrobj_t *obj, long value);
int pstrobj_set_float(pstrobj_t *obj, float value);
int pstrobj_set_double(pstrobj_t *obj, double value);
int pstrobj_set_list(pstrobj_t *obj);
int pstrobj_set_dict(pstrobj_t *obj);
```

Following functions set the object's value and type.

**Errors**

EINVAL, ENOMEM.

<a id="pstrobj_expect_"></a>

## `pstrobj_expect_*`

```c
void pstrobj_expect_null(pstrobj_t *obj, int *status);
int pstrobj_expect_bool(pstrobj_t *obj, int *status);
int pstrobj_expect_int(pstrobj_t *obj, int *status);
long pstrobj_expect_long(pstrobj_t *obj, int *status);
float pstrobj_expect_float(pstrobj_t *obj, int *status);
double pstrobj_expect_double(pstrobj_t *obj, int *status);
const char *pstrobj_expect_string(pstrobj_t *obj, int *status);
pstring_t *pstrobj_expect_pstring(pstrobj_t *obj, int *status);
pstrobj_t *pstrobj_expect_list(pstrobj_t *obj, int *status);
pstrobj_t *pstrobj_expect_dict(pstrobj_t *obj, int *status);
```

Following functions return the object's value if `obj` has a matching type.
Otherwise a default value is returned and `status` is set to an error code.

<a id="pstrobj_query_"></a>

## `pstrobj_query_*`

```c
#define PSTROBJ__IMPL_QUERY(NAME, TYPE)
PSTROBJ__IMPL_QUERY(bool, int);
PSTROBJ__IMPL_QUERY(int, int);
PSTROBJ__IMPL_QUERY(long, long);
PSTROBJ__IMPL_QUERY(float, float);
PSTROBJ__IMPL_QUERY(double, double);
PSTROBJ__IMPL_QUERY(string, const char *);
PSTROBJ__IMPL_QUERY(pstring, pstring_t *);
PSTROBJ__IMPL_QUERY(list, pstrobj_t *);
PSTROBJ__IMPL_QUERY(dict, pstrobj_t *);
```

Following functions query `obj` for a child and return the child's value
if `obj` has a matching type. Otherwise a default value is returned and
`status` is set to an error code.

<a id="pstrobj_get_"></a>

## `pstrobj_get_*`

```c
int pstrobj_get_int(pstrobj_t *obj, int def);
long pstrobj_get_long(pstrobj_t *obj, long def);
float pstrobj_get_float(pstrobj_t *obj, float def);
double pstrobj_get_double(pstrobj_t *obj, double def);
```

Following functions return the object's value or the default value `def`,
depending on the type of `obj`, converting between types if necessary.

<a id="pstrobj_copy_string"></a>

## `pstrobj_copy_string`

```c
int pstrobj_copy_string(pstrobj_t *obj, const char *str, size_t len);
int pstrobj_copy_pstring(pstrobj_t *obj, const pstring_t *str);
int pstrobj_copy_key(pstrobj_t *obj, const pstring_t *str);
int pstrobj_copy_keys(pstrobj_t *obj, const char *str, size_t len);
```

Copies the contents of `str` as the value of `obj`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrobj_wrap_string"></a>

## `pstrobj_wrap_string`

```c
int pstrobj_wrap_string(pstrobj_t *obj, const char *str, size_t len);
int pstrobj_wrap_pstring(pstrobj_t *obj, const pstring_t *str);
int pstrobj_wrap_key(pstrobj_t *obj, const pstring_t *str);
int pstrobj_wrap_keys(pstrobj_t *obj, const char *str, size_t len);
```

Wraps the contents of `str` as the value of `obj`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrobj_list_insert"></a>

## `pstrobj_list_insert`

```c
int pstrobj_list_insert(pstrobj_t *list, pstrobj_t *item, size_t i);
```

Inserts `item` at index `i` inside `list`.

**Errors**

EINVAL, ENOMEM.

<a id="pstrobj_list_remove"></a>

## `pstrobj_list_remove`

```c
pstrobj_t *pstrobj_list_remove(pstrobj_t *list, size_t i);
```

Removes item at index `i` in `list` and returns it.

<a id="pstrobj_list_free"></a>

## `pstrobj_list_free`

```c
PSTR_INLINE int pstrobj_list_free(pstrobj_t *list, size_t i);
```

Removes item at index `i` in `list` and frees it.

**Errors**

EINVAL.

<a id="pstrobj_dict_get"></a>

## `pstrobj_dict_get`

```c
pstrobj_t *pstrobj_dict_get(pstrobj_t *dict, const pstring_t *key);
pstrobj_t *pstrobj_dict_gets(
    pstrobj_t *dict, const char *key, size_t length
);
```

Finds an item in `dict` with specified `key`.

<a id="pstrobj_dict_insert"></a>

## `pstrobj_dict_insert`

```c
int pstrobj_dict_insert(pstrobj_t *dict, pstrobj_t *item);
```

Inserts `item` into `dict` with a previously set key.

**Errors**

EINVAL, ENOMEM.

<a id="pstrobj_dict_remove"></a>

## `pstrobj_dict_remove`

```c
pstrobj_t *pstrobj_dict_remove(
    pstrobj_t *dict, const char *key, size_t length
);
```

Removes item with specified key in `dict` and returns it.

<a id="pstrobj_dict_free"></a>

## `pstrobj_dict_free`

```c
PSTR_INLINE int pstrobj_dict_free(
    pstrobj_t *dict, const char *key, size_t length
);
```

Removes item with specified key in `dict` and frees it.

**Errors**

EINVAL.

