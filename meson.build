# Copyright 2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

project('pstring', 'c')

src = [
    'src/dictionary.c',
    'src/encoding.c',
    'src/io.c',
    'src/pattern.c',
    'src/pstring.c',
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

tests = executable(
    'pstring-test',
    dependencies: [pstring_dep, cpolyfill_dep],
    sources: [
        'test/dictionary.c',
        'test/encoding.c',
        'test/io.c',
        'test/main.c',
        'test/pattern.c',
        'test/pstring.c',
    ]
)

install_headers(
    'include/pstring/dictionary.h',
    'include/pstring/encoding.h',
    'include/pstring/io.h',
    'include/pstring/pattern.h',
    'include/pstring/pstring.h',
    subdir: 'pstring'
)

install_man(
    'docs/pstring.3'
)

test('pstring/pstring', tests, args: ['pstring'], protocol: 'tap')
test('pstring/dictionary', tests, args: ['dictionary'], protocol: 'tap')
test('pstring/encoding', tests, args: ['encoding'], protocol: 'tap')
test('pstring/io', tests, args: ['io'], protocol: 'tap')
test('pstring/pattern', tests, args: ['pattern'], protocol: 'tap')
