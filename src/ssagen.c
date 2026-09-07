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
#include "../include/error.h"
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

typedef struct {
        ILBuilder *b;
        HashMap *slots; /* name -> Ref SLOT */
} Ssagen;

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
static Ref emit_expr(Ssagen *s, ASTnode *n);
static void emit_stmt(Ssagen *s, ASTnode *n);

/* --- MAIN --- */
// selects the feather codegen target and optimization level from the CLI.
void ssagen_apply_options(const char *target, int level) {
        optlevel = level;
        if (target == NULL) {
                T = Deftgt; // host default (feather/config.h)
                return;
        }
        // mirrors feather's -t lookup (see feather/main.c)
        Target *targets[] = {
            &T_amd64_sysv,
            &T_amd64_apple,
            &T_amd64_win,
            &T_arm64,
            &T_arm64_apple,
            &T_rv64,
            NULL,
        };
        for (int i = 0; targets[i] != NULL; i++) {
                if (strcmp(target, targets[i]->name) == 0) {
                        T = *targets[i];
                        return;
                }
        }
        quil_error(STAGE_FILE, ERR_INVALID_TARGET, target);
}
Fn *ssagen_build(ASTnode *prog) {
        //
}
void ssagen_emit_asm(Fn *fn, FILE *out) {
        //
}
