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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// would have been defined feather/main.c
Target T;
extern Target T_amd64_sysv;
extern Target T_amd64_apple;
extern Target T_amd64_win;
extern Target T_arm64;
extern Target T_arm64_apple;
extern Target T_rv64;
int optlevel = 0; // no optimization by default
char debug['Z' + 1];

typedef struct {
        ILBuilder *ilb;
        HashMap *slots; /* name -> Ref SLOT */
        IlModule *mod;
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
static Ref emit_expr(Ssagen *s, ASTnode *n) {
        switch (n->type) {
        /* Literals */
        case NODE_INT_LITERAL: {
                // int8/int16/bool are still Kw (32b value, stored via Ostoreb/Ostoreh)
                int cls = quil_to_cls(n->resolved_type);
                if (cls == Kl) return il_const_int_l(s->ilb, (int64_t)n->data.int_literal.value);
                return il_const_int_w(s->ilb, n->data.int_literal.value);
        }
        case NODE_FLOAT_LITERAL: {
                int cls = quil_to_cls(n->resolved_type);
                if (cls == Kd) return il_const_float_d(s->ilb, n->data.float_literal.value);
                return il_const_float_s(s->ilb, (float)n->data.float_literal.value);
        }
        case NODE_BOOL_LITERAL:
                return il_const_int_w(s->ilb, n->data.bool_literal.value ? 1 : 0);
        case NODE_CHAR_LITERAL:
                return il_const_int_w(s->ilb, (int)n->data.char_literal.value);
        case NODE_STRING_LITERAL: {
                // data .str(str_id) = {b str b 0}
                static int str_id = 0;
                char name[32];
                // generate distinct data name each time
                snprintf(name, sizeof(name), ".str%d", str_id++);
                Lnk lnk = {0};
                IlData *data = il_data_begin(name, &lnk);
                il_data_add_str(data, DB, n->data.string_literal.value);
                il_data_add_b(data, 0);
                il_data_end(data);
                il_module_add_data(s->mod, data);
                // return address of data
                return il_global_sym(s->ilb, name);
        }
        case NODE_IDENTIFIER: {
                bool found;
                Ref *slotp = hashmap_get(s->slots, n->data.identifier.name, &found);
                if (!found) quil_error(STAGE_CODEGEN, ERR_UNDECLARED_VAR, n->data.identifier.name);
                Ref slot = *slotp;
                int cls = quil_to_cls(n->resolved_type);
                if (cls == Kl) return il_create_load_l(s->ilb, slot);
                if (cls == Ks) return il_create_load_s(s->ilb, slot);
                if (cls == Kd) return il_create_load_d(s->ilb, slot);
                return il_create_load_w(s->ilb, slot); // Kw bool char int8/16/32
        }
        /* binray unary ternary expressions */
        case NODE_BINARY_EXPRESSION: {
                Ref l = emit_expr(s, n->data.binary_expression.left);
                Ref r = emit_expr(s, n->data.binary_expression.right);
                tokenType op = n->data.binary_expression.op;
                int cls = quil_to_cls(n->resolved_type);
                if (cls == Kd) {
                        switch (op) {
                        case TOKEN_PLUS: return il_create_add_d(s->ilb, l, r);
                        case TOKEN_MINUS: return il_create_sub_d(s->ilb, l, r);
                        case TOKEN_STAR: return il_create_mul_d(s->ilb, l, r);
                        case TOKEN_FSLASH: return il_create_div_d(s->ilb, l, r);
                        default: break;
                        }
                } else if (cls == Ks) {
                        switch (op) {
                        case TOKEN_PLUS: return il_create_add_s(s->ilb, l, r);
                        case TOKEN_MINUS: return il_create_sub_s(s->ilb, l, r);
                        case TOKEN_STAR: return il_create_mul_s(s->ilb, l, r);
                        case TOKEN_FSLASH: return il_create_div_s(s->ilb, l, r);
                        default: break;
                        }
                } else if (cls == Kl) {
                        switch (op) {
                        case TOKEN_PLUS: return il_create_add_l(s->ilb, l, r);
                        case TOKEN_MINUS: return il_create_sub_l(s->ilb, l, r);
                        case TOKEN_STAR: return il_create_mul_l(s->ilb, l, r);
                        case TOKEN_FSLASH: return il_create_div_l(s->ilb, l, r);
                        case TOKEN_PERCENT: return il_create_rem_l(s->ilb, l, r);
                        case TOKEN_AND: return il_create_and_l(s->ilb, l, r);
                        case TOKEN_PIPE: return il_create_or_l(s->ilb, l, r);
                        case TOKEN_CARET: return il_create_xor_l(s->ilb, l, r);
                        default: break;
                        }
                        // comparisons
                        if (op == TOKEN_EEQUAL) return il_create_icmp_eq_l(s->ilb, l, r);
                        if (op == TOKEN_NEQUAL) return il_create_icmp_ne_l(s->ilb, l, r);
                        if (op == TOKEN_LABRACKET) return il_create_icmp_slt_l(s->ilb, l, r);
                        if (op == TOKEN_RABRACKET) return il_create_icmp_sgt_l(s->ilb, l, r);
                        if (op == TOKEN_LEQUAL) return il_create_icmp_sle_l(s->ilb, l, r);
                        if (op == TOKEN_GEQUAL) return il_create_icmp_sge_l(s->ilb, l, r);
                } else /* Kw */ {
                        switch (op) {
                        case TOKEN_PLUS: return il_create_add_w(s->ilb, l, r);
                        case TOKEN_MINUS: return il_create_sub_w(s->ilb, l, r);
                        case TOKEN_STAR: return il_create_mul_w(s->ilb, l, r);
                        case TOKEN_FSLASH: return il_create_div_w(s->ilb, l, r);
                        case TOKEN_PERCENT: return il_create_rem_w(s->ilb, l, r);
                        case TOKEN_AND: return il_create_and_w(s->ilb, l, r);
                        case TOKEN_PIPE: return il_create_or_w(s->ilb, l, r);
                        case TOKEN_CARET: return il_create_xor_w(s->ilb, l, r);
                        default: break;
                        }
                        if (op == TOKEN_EEQUAL) return il_create_icmp_eq_w(s->ilb, l, r);
                        if (op == TOKEN_NEQUAL) return il_create_icmp_ne_w(s->ilb, l, r);
                        if (op == TOKEN_LABRACKET) return il_create_icmp_slt_w(s->ilb, l, r);
                        if (op == TOKEN_RABRACKET) return il_create_icmp_sgt_w(s->ilb, l, r);
                        if (op == TOKEN_LEQUAL) return il_create_icmp_sle_w(s->ilb, l, r);
                        if (op == TOKEN_GEQUAL) return il_create_icmp_sge_w(s->ilb, l, r);
                        if (op == TOKEN_OR) return il_create_or_w(s->ilb, l, r);
                        if (op == TOKEN_AND) return il_create_and_w(s->ilb, l, r);
                }
                return l;
        }
        case NODE_UNARY_EXPRESSION: {
                Ref v = emit_expr(s, n->data.unary_expression.left);
                tokenType op = n->data.unary_expression.op;
                int cls = quil_to_cls(n->resolved_type);
                if (op == TOKEN_MINUS) {
                        if (cls == Kd) return il_create_neg_d(s->ilb, v);
                        if (cls == Ks) return il_create_neg_s(s->ilb, v);
                        if (cls == Kl) return il_create_neg_l(s->ilb, v);
                        return il_create_neg_w(s->ilb, v);
                }
                if (op == TOKEN_EXCLAMATION) {
                        // !a  ->  a == 0  (Kw bool 0/1)
                        return il_create_icmp_eq_w(s->ilb, v, il_const_zero(s->ilb));
                }
                quil_error(STAGE_CODEGEN, ERR_UNKNOWN, lexer_token_type_to_string(op));
        }
        case NODE_TERNARY_EXPRESSION: {
                Ref cond = emit_expr(s, n->data.ternary_expression.condition);
                Blk *then_blk = il_create_block(s->ilb, "tern.then");
                Blk *else_blk = il_create_block(s->ilb, "tern.else");
                Blk *merge = il_create_block(s->ilb, "tern.merge");
                il_create_cond_br(s->ilb, cond, then_blk, else_blk); // cur terminated

                // then
                il_set_insert_point(s->ilb, then_blk);
                Ref tv = emit_expr(s, n->data.ternary_expression.then_expr);
                il_create_br(s->ilb, merge);
                // else
                il_set_insert_point(s->ilb, else_blk);
                Ref ev = emit_expr(s, n->data.ternary_expression.else_expr);
                il_create_br(s->ilb, merge);

                // merge phi
                il_set_insert_point(s->ilb, merge);
                Blk *preds[] = {then_blk, else_blk};
                Ref vals[] = {tv, ev};
                int cls = quil_to_cls(n->resolved_type);
                if (cls == Kl) return il_create_phi_l(s->ilb, preds, vals, 2);
                if (cls == Kd) return il_create_phi_d(s->ilb, preds, vals, 2);
                if (cls == Ks) return il_create_phi_s(s->ilb, preds, vals, 2);
                return il_create_phi_w(s->ilb, preds, vals, 2);
        }
        default:
                quil_error(STAGE_CODEGEN, ERR_UNKNOWN, node_type_name(n->type));
        }
}
static void emit_stmt(Ssagen *s, ASTnode *n);

/* --- MAIN --- */
// selects the feather codegen target and optimization level from the CLI.
void ssagen_apply_options(const char *target, int level) {
        optlevel = level;
        if (target == NULL) {
                T = Deftgt; // host default (feather/config.h)
                return;
        }
        // mirrors feather's -t lookup
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
        IlModule *mod = il_module_create();
        Ssagen sg = {.ilb = NULL, .mod = mod, .slots = NULL};
}
void ssagen_emit_asm(IlModule *mod, FILE *out) {
        il_module_emit(mod, out);
}
