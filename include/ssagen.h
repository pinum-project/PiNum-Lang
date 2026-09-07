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

#include "../feather/filapi/filapi.h"
#include "_hashmap.h"
#include "ast.h"

// applies the --target/--optlevel CLI options to the feather backend:
// selects T (NULL = host default from feather/config.h) and sets optlevel.
void ssagen_apply_options(const char *target, int optlevel);

Fn *ssagen_build(ASTnode *prog);
void ssagen_emit_asm(Fn *fn, FILE *out);

#endif // !SSAGEN_H
