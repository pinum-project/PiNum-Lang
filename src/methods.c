/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#include "../include/methods.h"
#include <string.h>

const method_def METHODS[] = {
    // vec removed - stdlib will provide vec.quil later
};
const int METHOD_COUNT = 0;

// NOTE: hashmap will be implimented when the METHODS exceed 32 elements
const method_def *method_lookup(const char *obj_type, const char *name) {
        for (int i = 0; i < METHOD_COUNT; i++) {
                if (strncmp(obj_type, METHODS[i].type_prefix, strlen(METHODS[i].type_prefix)) == 0 &&
                    strcmp(name, METHODS[i].name) == 0) {
                        return &METHODS[i];
                }
        }
        return NULL;
}
