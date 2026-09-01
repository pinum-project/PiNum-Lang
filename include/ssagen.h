/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#ifndef SSAGEN_H
#define SSAGEN_H

#include "ast.h"

struct Fn;
typedef struct Fn Fn;
Fn *ssagen_build(ASTnode *prog);             // return Fn type in memory
void ssagen_emit_asm(Fn *fn, FILE *asm_out); // Fn* -> .s

#endif // !SSAGEN_H
