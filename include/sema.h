/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#ifndef SEMA_H
#define SEMA_H

#include "_hashmap.h"
#include "ast.h"
#include "lexer.h"
#include <stddef.h>

typedef struct {
        HashMap **frames;      // stack of symbol tables
        HashMap *functions;    // table for function type and name
        size_t frame_count;    // number of scopes currently open
        size_t frame_capacity; // allocated slots
} SemAnalyzer;

// function signatures
typedef struct {
        // we do not need to hold name in the signature, the hashmap key will be the name
        char *return_type;
        char **param_types;
        size_t param_count;
} funcSig;

// entry point
void semantic_analyze(ASTnode *program);

#endif // !SEMA_H
