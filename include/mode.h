/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#ifndef MODE_H
#define MODE_H

#include "lexer.h"
#include <stdbool.h>

// engine mode
extern bool ENGINE_MODE;
// quote modes
extern bool SQUOTE_MODE; // single quote mode
extern bool DQUOTE_MODE; // duble quote mode

// checks if ENGINE_MODE can be enabled or not
bool check_program_mode(token_list *list);

#endif // !MODE_H

// NOTE: All the function in this file will be defined in ../src/helper.c
