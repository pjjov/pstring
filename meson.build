# Copyright 2025-2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

project('pstring', 'c', version: '1.4')

src = [
    'src/encoding/base32.c',
    'src/encoding/base64.c',
    'src/encoding/cstring.c',
    'src/encoding/hex.c',
    'src/encoding/json.c',
    'src/encoding/url.c',
    'src/encoding/utf8.c',
    'src/encoding/utf16.c',
    'src/encoding/xml.c',
    'src/format/json.c',
    'src/format/xml.c',
    'src/core.c',
    'src/encoding.c',
    'src/eval.c',
    'src/format.c',
    'src/glob.c',
    'src/global.c',
    'src/io.c',
    'src/object.c',
    'src/search.c',
    'src/simd.c',
    'src/transform.c',
    'src/wordexp.c',
]

args = []
inc = include_directories('./include/')

cc = meson.get_compiler('c')

allocator_dep = dependency(
    'allocator_t',
    required: not cc.has_header('allocator.h')
)

cpolyfill_dep = dependency('cpolyfill', required: true)
xxhash_dep = dependency('xxhash', 'libxxhash', required: false)

if xxhash_dep.found()
    args += '-DPSTRING_USE_XXHASH'
endif

lib = library(
    'pstring',
    c_args: args,
    dependencies: [allocator_dep, cpolyfill_dep, xxhash_dep],
    sources: src,
    include_directories: inc,
    install: true
)

pstring_dep = declare_dependency(
    link_with: [lib],
    include_directories: inc
)

meson.override_dependency('pstring', pstring_dep)

install_headers(
    'include/pstring/core.h',
    'include/pstring/encoding.h',
    'include/pstring/eval.h',
    'include/pstring/format.h',
    'include/pstring/glob.h',
    'include/pstring/io.h',
    'include/pstring/object.h',
    'include/pstring/pstring.h',
    'include/pstring/search.h',
    'include/pstring/transform.h',
    'include/pstring/wordexp.h',
    subdir: 'pstring'
)

install_man(
    'docs/pstring.3',
    'docs/pstring-encoding.3',
    'docs/pstring-object.3'
)

if not meson.is_subproject()
    tests = executable(
        'pstring-test',
        install: false,
        dependencies: [pstring_dep, cpolyfill_dep],
        sources: [
            'test/encoding.c',
            'test/eval.c',
            'test/glob.c',
            'test/io.c',
            'test/main.c',
            'test/object.c',
            'test/pstring.c',
            'test/wordexp.c',
        ]
    )

    test('pstring', tests, args: ['--tap'], protocol: 'tap')
endif
