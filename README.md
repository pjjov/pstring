<!--
Copyright 2025 Predrag Jovanović
SPDX-FileCopyrightText: 2025 Predrag Jovanović
SPDX-License-Identifier: Apache-2.0
-->

# pstring - fully-featured string library for C

**pstring** is a modern string library for C, offering a wide range
of features such as streams, dictionaries, encodings, pattern matching
and much more. Unlike many existing string libraries which strive for
minimalism, **pstring** attempts to provide a complete set of utilities
for dealing with strings in C.

```c
#include <pstring/encoding.h>
#include <pstring/io.h>
#include <pstring/pstring.h>

void example(pstream_t *out) {
    pstring_t *str = { 0 };
    pstrnew(&str, "Hi", 0, NULL)

    pstrcats(&str, " , world!", 0);
    pstrrepl(&str, PSTR("Hi"), PSTR("Hello"), 0);

    pstring_t encoded = { 0 };
    pstrenc_base64(&encoded, &str);

    pstream_write(out, pstrbuf(&encoded), pstrlen(&encoded));
    pstrfree(&str);
}
```

## Usage

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

String functions are grouped in separate header files:
- `pstring.h` - common string operations.
- `encoding.h` - encoding and decoding functions.
- `io.h` - file, memory and custom streams.
- `pstrdict.h` - hash map that stores key-value pairs.

## Building

**pstring** relies on Meson for building:

```sh
meson setup build && cd build
meson compile
```

## License

See [LICENSE](./LICENSE) file for more information.
