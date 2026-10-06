# Copyright 2025-2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

# The adapters are header-only: the library never links against these,
# they are only needed to test the adapters themselves.

test_args = []

test_deps = [
    pstring_dep,
    cpolyfill_dep,
    allocator_dep,
]

pcre2_dep = dependency(
    'libpcre2-8',
    required: false,
)

if pcre2_dep.found()
    test_args += '-DPSTRING_TEST_PCRE2'
    test_deps += pcre2_dep
endif

onig_dep = dependency(
    'oniguruma',
    required: get_option('test_onig'),
)

if onig_dep.found()
    test_args += '-DPSTRING_TEST_ONIG'
    test_deps += onig_dep
endif

tests = executable(
    'pstring-test',
    sources: files(
        'encoding.c',
        'eval.c',
        'glob.c',
        'io.c',
        'main.c',
        'object.c',
        'pattern.c',
        'pstring.c',
        'wordexp.c',
    ),
    c_args: test_args,
    dependencies: test_deps,
    install: false,
)

test(
    'pstring',
    tests,
    args: ['--tap'],
    protocol: 'tap',
)
