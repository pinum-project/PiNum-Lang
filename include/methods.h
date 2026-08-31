/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#ifndef METHODS_H
#define METHODS_H

typedef struct {
        const char *type_prefix;
        const char *name;
        const char *c_helper;
        const char *specifier;
} method_def;

extern const method_def METHODS[];
extern const int METHOD_COUNT;

const method_def *method_lookup(const char *obj_type, const char *name);

#endif // !METHODS_H
