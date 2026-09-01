/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

/* .ssa code generator */

#include "../include/ssagen.h"
// keep feather private here; include only when we link feather objects
// #include "../feather/all.h"
// #include "../feather/config.h"

Fn *ssagen_build(ASTnode *prog) {
        (void)prog;
        return NULL; // TODO: allocate Fn via feather's emalloc/newblk once feather is linked
}

void ssagen_emit_asm(Fn *fn, FILE *out) {
        (void)fn;
        (void)out;
        // TODO: T = Deftgt; T.abi0(fn); fillcfg(fn); ssa(fn); gvn(fn); gcm(fn); T.abi1(fn); T.isel(fn); rega(fn); T.emitfn(fn, out);
}
