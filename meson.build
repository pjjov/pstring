# Copyright 2025-2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

project(
    'pstring',
    'c',
    version: '1.5',
)

subdir('src')

install_headers(
    'include/pstring/core.h',
    'include/pstring/encoding.h',
    'include/pstring/eval.h',
    'include/pstring/format.h',
    'include/pstring/glob.h',
    'include/pstring/io.h',
    'include/pstring/object.h',
    'include/pstring/pattern.h',
    'include/pstring/pstring.h',
    'include/pstring/search.h',
    'include/pstring/transform.h',
    'include/pstring/wordexp.h',
    subdir: 'pstring',
)

install_headers(
    'include/pstring/pattern/onig.h',
    'include/pstring/pattern/pcre2.h',
    'include/pstring/pattern/posix.h',
    subdir: 'pstring/pattern',
)

install_man(
    'docs/pstring.3',
    'docs/pstring-encoding.3',
    'docs/pstring-object.3',
)

if not meson.is_subproject()
    subdir('test')
endif
