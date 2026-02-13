/*  pstring - fully-featured string library for C

    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Copyright 2026 Предраг Јовановић

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef PSTRING_PATTERN_H
#define PSTRING_PATTERN_H

typedef struct allocator_t allocator_t;
typedef struct pstrexpr_t pstrexpr_t;

pstrexpr_t *pstrexpr_new(const char *pattern, allocator_t *allocator);
void pstrexpr_free(pstrexpr_t *expr);

#endif
