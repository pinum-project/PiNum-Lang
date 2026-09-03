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
#include "../feather/config.h"
#include <complex.h>
#include <stdint.h>
#include <string.h>

extern Target T_amd64_sysv;
extern Target T_amd64_apple;
extern Target T_amd64_win;
extern Target T_arm64;
extern Target T_arm64_apple;
extern Target T_rv64;
// would have been defined feather/main.c
Target T;
int optlevel = 0; // no optimization by default
char debug['Z' + 1];

/* --- HELPER --- */
// enum { Kx=-1, Kw, Kl, Ks, Kd };
static int quil_to_cls(const char *t) {
        if (!t) return Kw;
        if (!strcmp(t, "int8") || !strcmp(t, "int16") || !strcmp(t, "int32") ||
            !strcmp(t, "bool") || !strcmp(t, "uint8") || !strcmp(t, "uint16") ||
            !strcmp(t, "uint32")) {
                return Kw;
        }
        if (!strcmp(t, "int64") || !strcmp(t, "uint64") || !strcmp(t, "string") ||
            !strcmp(t, "char*")) {
                return Kl; // pointers are l
        }
        if (!strcmp(t, "char")) return Kw; // char literal is w (byte promoted)
        if (!strcmp(t, "float32")) return Ks;
        if (!strcmp(t, "float64")) return Kd;
        return Kw;
}
static Ref new_tmp(SsaGen *g, const int cls) {
        return newtmp("t", cls, g->fn);
}

static int str_id = 0;
static HashMap *str_cache;
static Ref emit_expr(SsaGen *g, ASTnode *n) {
        switch (n->type) {
        // --- Literals and Identifiers ---
        case NODE_INT_LITERAL:
                // sema sets resolved_type "int32"/"int64", else Kw
                return getcon(n->data.int_literal.value, g->fn);
        case NODE_FLOAT_LITERAL: {
                Con c = {.type = CBits, .bits.d = n->data.float_literal.value};
                if (quil_to_cls(n->resolved_type) == Ks) {
                        c.bits.s = (float)c.bits.d;
                        c.flt = 1;
                        c.bits.i = (int)c.bits.d;
                } else {
                        c.flt = 2; // Kd
                }
                return newcon(&c, g->fn);
        }
        case NODE_BOOL_LITERAL:
                return getcon(n->data.bool_literal.value ? 1 : 0, g->fn);
        case NODE_CHAR_LITERAL:
                return getcon((int64_t)n->data.char_literal.value, g->fn);
        case NODE_STRING_LITERAL: {
                //
        }
        case NODE_IDENTIFIER:
        case NODE_ARRAY_ACCESS:

        default:
                die("emit_expr todo %s", node_type_name(n->type));
        }
}

/* --- MAIN --- */
Fn *ssagen_build(ASTnode *prog) {
        (void)prog;
        T = Deftgt; // dynamic host arch (feather/config.h)
        // make empty fn like feather/parse.c:970-990
        Fn *fn = alloc(sizeof(Fn));
        memset(fn, 0, sizeof(Fn));
        fn->ntmp = 0;
        fn->ncon = 2;
        fn->tmp = vnew(0, sizeof(Tmp), PFn);
        fn->con = vnew(2, sizeof(Con), PFn);
        for (int i = 0; i < Tmp0; i++) {
                if (T.fpr0 <= i && i < T.fpr0 + T.nfpr) {
                        newtmp(0, Kd, fn);
                } else {
                        newtmp(0, Kl, fn);
                }
        }
        // i have no idea what these does, i just copied feather/parse.c
        fn->con[0].type = CBits;
        fn->con[0].bits.i = 0xdeaddead; // UNDEF
        fn->con[1].type = CBits;
        fn->con[1].bits.i = 0; // 0
        fn->name = "main";
        fn->retty = Kx; // -1 = simple w return, not typ[0]
        fn->lnk.export = 1;
        fn->leaf = 1;

        // make first block
        Blk *b = newblk();
        b->name = "start";
        b->id = 0;
        fn->start = b;
        fn->nblk = 1;

        // emit one instruction: return 42 for test
        Ref c42 = getcon(42, fn);
        b->jmp.type = Jretw; // return word
        b->jmp.arg = c42;

        fn->mem = vnew(0, sizeof(Mem), PFn);
        fn->nmem = 0;
        fn->rpo = vnew(1, sizeof(Blk *), PFn);
        fn->rpo[0] = b;
        return fn;
}
void ssagen_emit_asm(Fn *fn, FILE *out) {
        T.abi0(fn);
        fillcfg(fn);
        ssa(fn);
        T.abi1(fn);
        T.isel(fn);
        filllive(fn);
        spill(fn);
        rega(fn);
        T.emitfn(fn, out); // writes assembly
        freeall();
}
