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
#include <stdbool.h>
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
        char *cur_ns; /* current namespace("scope" keyword) */
} Ssagen;

/* --- Prototypes --- */
static int quil_to_cls(const char *t);
static Ref emit_expr(Ssagen *s, ASTnode *n);
static void emit_stmt(Ssagen *s, ASTnode *n);
static char *mangle(const char *qname);
static void emit_func(Ssagen *s, ASTnode *fndef);

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
static void emit_stmt(Ssagen *s, ASTnode *n) {
        switch (n->type) {
        case NODE_VAR_DECL: {
                int cls = quil_to_cls(n->data.var_decl.type_name);
                const char *t = n->data.var_decl.type_name;
                int elem_size = 4;
                if (!strcmp(t, "int8") || !strcmp(t, "uint8") || !strcmp(t, "char") || !strcmp(t, "bool")) elem_size = 1;
                else if (!strcmp(t, "int16") || !strcmp(t, "uint16")) elem_size = 2;
                else if (!strcmp(t, "int32") || !strcmp(t, "uint32") || !strcmp(t, "float32")) elem_size = 4;
                else if (!strcmp(t, "int64") || !strcmp(t, "uint64") || !strcmp(t, "float64") || !strcmp(t, "string")) elem_size = 8;
                int sz = elem_size;
                if (n->data.var_decl.is_array) sz *= n->data.var_decl.array_size;
                else if (cls == Kl || cls == Kd) sz = 8; // scalar Kl/Kd still 8

                Ref slot = il_create_alloc4(s->ilb, il_const_int_w(s->ilb, sz)); // always l
                Ref *rp = emalloc(sizeof(Ref));
                *rp = slot;
                hashmap_put(s->slots, strdup(n->data.var_decl.name), rp);
                if (n->data.var_decl.value) {
                        Ref v = emit_expr(s, n->data.var_decl.value);
                        il_create_store(s->ilb, cls, v, slot);
                }
                break;
        }
        case NODE_ASSIGN: {
                bool found;
                Ref *sp = hashmap_get(s->slots, n->data.assign.name, &found);
                if (!found) quil_error(STAGE_CODEGEN, ERR_UNDECLARED_VAR, n->data.assign.name);
                Ref slot = *sp;
                Ref v = emit_expr(s, n->data.assign.value);
                if (n->data.assign.index) {
                        Ref idx = emit_expr(s, n->data.assign.index);
                        int idx_cls = quil_to_cls(n->data.assign.index->resolved_type);
                        int elem_cls = quil_to_cls(n->resolved_type);
                        const char *et = n->resolved_type ? n->resolved_type : "int32";
                        int elem_size = 4;
                        if (!strcmp(et, "int8") || !strcmp(et, "uint8") || !strcmp(et, "char") || !strcmp(et, "bool")) elem_size = 1;
                        else if (!strcmp(et, "int16") || !strcmp(et, "uint16")) elem_size = 2;
                        else if (!strcmp(et, "int32") || !strcmp(et, "uint32") || !strcmp(et, "float32")) elem_size = 4;
                        else if (!strcmp(et, "int64") || !strcmp(et, "uint64") || !strcmp(et, "float64") || !strcmp(et, "string")) elem_size = 8;
                        Ref off;
                        if (idx_cls == Kl) {
                                off = il_create_mul_l(s->ilb, idx, il_const_int_l(s->ilb, elem_size));
                        } else {
                                off = il_create_mul_w(s->ilb, idx, il_const_int_w(s->ilb, elem_size));
                        }
                        Ref off_l = (idx_cls == Kl) ? off : il_create_extsw_l(s->ilb, off);
                        Ref addr = il_create_add_l(s->ilb, slot, off_l);
                        il_create_store(s->ilb, elem_cls, v, addr);
                } else {
                        int cls = quil_to_cls(n->resolved_type ? n->resolved_type : n->data.assign.value->resolved_type);
                        if (!cls) cls = quil_to_cls("int32");
                        il_create_store(s->ilb, cls, v, slot);
                }
                break;
        }
        case NODE_RETURN: {
                if (!n->data.returns.expression) {
                        il_create_ret_void(s->ilb);
                } else {
                        Ref v = emit_expr(s, n->data.returns.expression);
                        int cls = quil_to_cls(n->data.returns.expression->resolved_type);
                        if (cls == Kl) il_create_ret_l(s->ilb, v);
                        else if (cls == Kd) il_create_ret_d(s->ilb, v);
                        else if (cls == Ks) il_create_ret_s(s->ilb, v);
                        else il_create_ret_w(s->ilb, v);
                }
                break;
        }
        case NODE_BLOCK: {
                for (int i = 0; i < n->data.blocks.count; i++) {
                        emit_stmt(s, n->data.blocks.statements[i]);
                }
                break;
        }
        case NODE_IF_STAT:
        case NODE_WHILE:
        case NODE_FOR:
        case NODE_PRINT:
        case NODE_READ:
                // TODO: implement control flow and IO later
                break;
        default:
                break;
        }
}

static char *mangle(const char *qname) {
        // "_" -> "_0", "::" -> "_1" so "std_foo" (std_0foo) != "std::foo" (std_1foo)
        // "a::b_c" -> "a_1b_0c"
        size_t n = strlen(qname);
        char *out = emalloc(n * 2 + 1);
        char *p = out;
        for (size_t i = 0; i < n;) {
                if (qname[i] == '_') {
                        *p++ = '_';
                        *p++ = '0';
                        i++;
                } else if (qname[i] == ':' && i + 1 < n && qname[i + 1] == ':') {
                        *p++ = '_';
                        *p++ = '1';
                        i += 2;
                } else {
                        *p++ = qname[i++];
                }
        }
        *p = '\0';
        return out;
}
static void emit_func(Ssagen *s, ASTnode *fndef) {
        if (fndef->data.func_def.is_extern) {
                return; // if its is extern then its prototype only function will be defined elsewhere
        }
        char *qname = s->cur_ns ? strf(PHeap, "%s::%s", s->cur_ns, fndef->data.func_def.name) : fndef->data.func_def.name;
        char *mangled = mangle(qname);
        qname = mangled;
        Lnk lnk = {.export = fndef->data.func_def.is_public || strcmp(fndef->data.func_def.name, "main") == 0}; // public fn main required
        Fn *fn = il_create_function(qname, Kx, &lnk);
        s->ilb = il_create(fn);
        Blk *entry = il_create_block(s->ilb, "entry");
        il_set_insert_point(s->ilb, entry); // ilbuilder.c:25 cur

        for (int i = 0; i < fndef->data.func_def.param_count; i++) {
                ASTnode *p = fndef->data.func_def.params[i];
                int cls = quil_to_cls(p->data.var_decl.type_name);
                Ref pr = il_add_param(s->ilb, cls); // Kw/Kl
                Ref slot = il_create_alloc4(s->ilb, il_const_int_w(s->ilb, 4));
                Ref *rp = emalloc(sizeof(Ref));
                *rp = slot;                                               // PHeap so survives freeall()
                hashmap_put(s->slots, strdup(p->data.var_decl.name), rp); // slots name->Ref Kl
                il_create_store(s->ilb, cls, pr, slot);                   // store param to slot
        }
        // body BLOCK include/ast.h:191 -> emit_stmt() for each stmt
        for (int i = 0; i < fndef->data.func_def.body->data.blocks.count; i++) {
                emit_stmt(s, fndef->data.func_def.body->data.blocks.statements[i]);
        }
        if (!s->ilb->cur || s->ilb->cur->jmp.type == Jxxx) il_create_ret_void(s->ilb); // if no ret
        fn = il_finish(s->ilb);                                                        // nblk + rpo
        il_module_add_function(s->mod, fn);
        s->ilb = NULL;
        hashmap_clear(s->slots);
}

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
IlModule *ssagen_build(ASTnode *prog) {
        IlModule *mod = il_module_create();
        Ssagen s = {.mod = mod, .slots = hashmap_create(hm_hash_str, hm_eq_str, NULL, NULL), .ilb = NULL};

        for (int i = 0; i < prog->data.program.count; i++) {
                ASTnode *stmt = prog->data.program.statements[i];
                if (stmt->type == NODE_NAMESPACE) { // "scope" is referred as namespace
                        char *old = s.cur_ns;
                        s.cur_ns = old ? strf(PHeap, "%s::%s", old, stmt->data.namespace_decl.name) : strdup(stmt->data.namespace_decl.name);

                        // recurse into namespace_decl.body BLOCK include/ast.h:245 BLOCK
                        for (int j = 0; j < stmt->data.namespace_decl.body->data.blocks.count; j++) {
                                emit_func(&s, stmt->data.namespace_decl.body->data.blocks.statements[j]);
                        }
                        s.cur_ns = old;
                } else if (stmt->type == NODE_FUNC_DEF) {
                        emit_func(&s, stmt);
                }
        }

        hashmap_free(s.slots);
        return mod;
}
void ssagen_emit_asm(IlModule *mod, FILE *out) {
        il_module_emit(mod, out);
}
void ssagen_emit_ssa(IlModule *mod, FILE *out) {
        // SSA text for inspection: data + functions via printfn() feather/parse.c:594
        for (uint i = 0; i < mod->ndat; i++) {
                IlData *d = mod->datas[i];
                for (uint j = 0; j < d->n; j++) {
                        // ssa data is same as asm data header: data $name = { ... }
                        // use feather's data printer via emitdat with ssa flag? For now reuse emitdat as ssa data is similar
                        // Instead, just emit via printfn's data path: use emitdat with out (ssa and asm share same data syntax)
                        emitdat(&d->items[j], out);
                }
                fputs("/* end data */\n\n", out);
        }
        for (uint i = 0; i < mod->nfn; i++) {
                printfn(mod->fns[i], out);
                fputs("\n", out);
        }
}
