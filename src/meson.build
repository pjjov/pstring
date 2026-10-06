# Copyright 2025-2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

args = []

inc = include_directories('../include')

cc = meson.get_compiler('c')

allocator_dep = dependency(
    'allocator_t',
    required: not cc.has_header('allocator.h'),
)

cpolyfill_dep = dependency('cpolyfill')
xxhash_dep = dependency(
    'xxhash',
    'libxxhash',
    required: false,
)

# pf_threads.h falls back to POSIX threads when C11 <threads.h> is missing.
thread_dep = dependency('threads')

if xxhash_dep.found()
    args += '-DPSTRING_USE_XXHASH'
endif

subdir('encoding')
subdir('format')

src = encoding_src + format_src + files(
    'core.c',
    'encoding.c',
    'eval.c',
    'format.c',
    'glob.c',
    'global.c',
    'io.c',
    'object.c',
    'pattern.c',
    'search.c',
    'simd.c',
    'transform.c',
    'wordexp.c',
)

lib = library(
    'pstring',
    sources: src,
    c_args: args,
    dependencies: [
        allocator_dep,
        cpolyfill_dep,
        xxhash_dep,
        thread_dep,
    ],
    include_directories: inc,
    install: true,
)

pstring_dep = declare_dependency(
    link_with: lib,
    dependencies: [thread_dep],
    include_directories: inc,
)

meson.override_dependency('pstring', pstring_dep)
