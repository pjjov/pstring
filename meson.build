# Copyright 2025 Predrag Jovanović
# SPDX-FileCopyrightText: 2025 Predrag Jovanović
# SPDX-License-Identifier: Apache-2.0

project('pstring', 'c')

src = [
    './src/encoding.c',
    './src/io.c',
    './src/pstring.c',
    './src/pstrdict.c',
]

inc = include_directories('./include/')

cpolyfill_dep = dependency('cpolyfill', required: true)

lib = library(
    'pstring',
    dependencies: cpolyfill_dep,
    sources: src,
    include_directories: inc
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
        'test/encoding.c',
        'test/io.c',
        'test/main.c',
        'test/pstring.c',
        'test/pstrdict.c',
    ]
)

test('pstring/pstring', tests, args: ['pstring'], protocol: 'tap')
test('pstring/dict', tests, args: ['dict'], protocol: 'tap')
test('pstring/encoding', tests, args: ['encoding'], protocol: 'tap')
test('pstring/io', tests, args: ['io'], protocol: 'tap')
