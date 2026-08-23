/********************************************************************
 *   _____ _ _   _                       _                          *
 *  |  __ (_) \ | |                     | |                         *
 *  | |__) ||  \| |_   _ _ __ ___ ______| |     __ _ _ __   __ _    *
 *  |  ___/ | . ` | | | | '_ ` _ \______| |    / _` | '_ \ / _` |   *
 *  | |   | | |\  | |_| | | | | | |     | |___| (_| | | | | (_| |   *
 *  |_|   | |_| \_|\__,_|_| |_| |_|     |______\__,_|_| |_|\__, |   *
 *                                                          __/ |   *
 *                                                         |___/    *
 *                                                                  *
 *  Copyright (c) 2026 tanvir-techbro.                              *
 *  You may opt to use, copy, modify, merge, publish, distribute    *
 *  and/or sell copies of the Software, and permit persons to whom  *
 *  the Software is furnished to do so, under the conditions of the *
 *  LICENSE.                                                        *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, *
 *  EXPRESS OR IMPLIED.                                             *
 *                                                                  *
 *  If you find any bug you would be highly encouraged to create a  *
 *  github issue at <https://github.com/pinum-project/PiNum-Lang>   *
 *  or contact <surjointelligence.team@gmail.com>                   *
 ********************************************************************/

/* C code generation backend */

#include "../include/codegen_c.h"
#include "../include/methods.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// function definitions
static void codegen_node(ASTnode *node, FILE *output, int level);
// like codegen_node, but adds a ';' when used as a bare statement
// (e.g. a NODE_FUNC_CALL used as a statement: foo();)
static void codegen_stmt(ASTnode *node, FILE *output, int level);

static bool is_char(ASTnode *node) {
        if (node->type == NODE_CHAR_LITERAL) {
                return true;
        }
        if (node->type == NODE_IDENTIFIER) {
                const char *type = node->resolved_type;
                return type && strcmp(type, "char") == 0;
        }
        return false;
}

static bool is_string(ASTnode *node) {
        if (node->type == NODE_STRING_LITERAL) {
                return true;
        }
        if (node->type == NODE_IDENTIFIER) {
                const char *type = node->resolved_type;
                return type && strcmp(type, "char *") == 0;
        }
        // "a" + b + "c" parses as ("a" + b) + "c"; the left side of the
        // outer + is itself a string-producing binary expression.
        if (node->type == NODE_BINARY_EXPRESSION) {
                ASTnode *left = node->data.binary_expression.left;
                ASTnode *right = node->data.binary_expression.right;
                tokenType op = node->data.binary_expression.op;
                // concat: "a" + "b" produces a string
                if (op == TOKEN_PLUS) {
                        return is_string(left) && is_string(right);
                }
                // repetition: "ab" * 3 or 'a' * 3 produces a string
                if (op == TOKEN_STAR) {
                        return is_string(left) || is_string(right) ||
                               is_char(left) || is_char(right);
                }
        }
        return false;
}

static const char *specifier_for_type(const char *type) {
        if (strcmp(type, "char *") == 0) return "%s";            // string
        if (strcmp(type, "int8") == 0 || strcmp(type, "int16") == 0 ||
            strcmp(type, "int32") == 0 || strcmp(type, "bool") == 0) return "%d";
        if (strcmp(type, "uint8") == 0 || strcmp(type, "uint16") == 0 ||
            strcmp(type, "uint32") == 0) return "%u";
        if (strcmp(type, "int64") == 0) return "%lld";
        if (strcmp(type, "uint64") == 0) return "%llu";
        if (strcmp(type, "float32") == 0 || strcmp(type, "float64") == 0) return "%f";
        if (strcmp(type, "char") == 0) return "%c";
        return "%d"; // vec_* use their own print helpers; fallback
}

// maps a PiNum type name to its C equivalent
static const char *codegen_type(const char *type_name) {
        if (strcmp(type_name, "int8") == 0) return "int8_t";
        if (strcmp(type_name, "int16") == 0) return "int16_t";
        if (strcmp(type_name, "int32") == 0) return "int32_t";
        if (strcmp(type_name, "int64") == 0) return "int64_t";
        if (strcmp(type_name, "uint8") == 0) return "uint8_t";
        if (strcmp(type_name, "uint16") == 0) return "uint16_t";
        if (strcmp(type_name, "uint32") == 0) return "uint32_t";
        if (strcmp(type_name, "uint64") == 0) return "uint64_t";
        if (strcmp(type_name, "float32") == 0) return "float";
        if (strcmp(type_name, "float64") == 0) return "double";
        if (strcmp(type_name, "string") == 0) return "char *";
        if (strcmp(type_name, "vec") == 0) return "vec";
        return type_name; // char, bool, vec_<T> map 1:1
}

// returns the printf format specifier that matches a node's value type
static const char *codegen_specifier(ASTnode *node) {
        switch (node->type) {
        case NODE_STRING_LITERAL: return "%s";
        case NODE_FLOAT_LITERAL: return "%f";
        case NODE_CHAR_LITERAL: return "%c";
        case NODE_BOOL_LITERAL: return "%d"; // true/false printed as 1/0
        case NODE_TERNARY_EXPRESSION: return codegen_specifier(node->data.ternary_expression.then_expr);
        case NODE_BINARY_EXPRESSION: {
                ASTnode *left = node->data.binary_expression.left;
                ASTnode *right = node->data.binary_expression.right;
                tokenType op = node->data.binary_expression.op;
                // a char repetition returns a char* (string), so print it as %s
                if (op == TOKEN_STAR && (is_char(left) || is_char(right))) {
                        return "%s";
                }
                // a string repetition returns a char* too
                if (op == TOKEN_STAR && (is_string(left) || is_string(right))) {
                        return "%s";
                }
                // adding two strings returns a char* too
                if (op == TOKEN_PLUS && is_string(left) && is_string(right)) {
                        return "%s";
                }
                return "%d";
        }
        case NODE_IDENTIFIER: {
                const char *type = node->resolved_type;
                return type ? specifier_for_type(type) : "%d";
        }
        case NODE_ARRAY_ACCESS:
                return specifier_for_type(node->resolved_type);
        case NODE_MEMBER_ACCESS:
                // properties like .size / .capacity are size_t
                return "%zu";
        case NODE_FUNC_CALL:
                return specifier_for_type(node->resolved_type ? node->resolved_type : "int32");
        default:
                return "%d";
        }
}

// maps a declaration to its concrete C type, resolving vec<T> → vec_T
static const char *codegen_decl_type(ASTnode *node) {
        if (node->data.var_decl.type_name && strcmp(node->data.var_decl.type_name, "vec") == 0) {
                static char buf[32];
                snprintf(buf, sizeof(buf), "vec_%s", node->data.var_decl.element_type ? node->data.var_decl.element_type : "int32");
                return buf;
        }
        return codegen_type(node->data.var_decl.type_name);
}

// true when a node holds a vec<T> value (resolved_type like "vec_int")
static bool is_vec(ASTnode *node) {
        const char *t = node->resolved_type;
        return t && strncmp(t, "vec_", 4) == 0;
}

// maps a PiNum operator token to its C equivalent
static const char *codegen_operator(tokenType op) {
        switch (op) {
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-"; // can be used as unary or binary operator
        case TOKEN_STAR: return "*";
        case TOKEN_FSLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EEQUAL: return "==";
        case TOKEN_NEQUAL: return "!=";
        case TOKEN_LABRACKET: return "<";
        case TOKEN_RABRACKET: return ">";
        case TOKEN_LEQUAL: return "<=";
        case TOKEN_GEQUAL: return ">=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_EXCLAMATION: return "!"; // unary operator
        default:
                return lexer_token_type_to_string(op);
        }
}

static void codegen_for_param(ASTnode *node, FILE *output, int level) {
        if (node == NULL) {
                return;
        }
        switch (node->type) {
        case NODE_VAR_DECL: {
                // build the full C type, e.g. "long int" or "char *" or "vec_int"
                const char *base_type = codegen_decl_type(node);
                char full_type[64];
                if (node->data.var_decl.modifiers) {
                        snprintf(full_type, sizeof(full_type), "%s %s", node->data.var_decl.modifiers, base_type);
                } else {
                        snprintf(full_type, sizeof(full_type), "%s", base_type);
                }
                // get the modifier
                if (node->data.var_decl.modifiers) {
                        fprintf(output, "%s ", node->data.var_decl.modifiers);
                }
                // get type and name
                fprintf(output, "%s %s", base_type, node->data.var_decl.name);
                // check if they have any value assigned
                if (node->data.var_decl.value) {
                        fprintf(output, " = ");
                        codegen_node(node->data.var_decl.value, output, level);
                }
                break;
        }
        case NODE_ASSIGN:
                if (node->data.assign.index) {
                        fprintf(output, "%s.data[__pinum_check_bounds(%s.size, ", node->data.assign.name, node->data.assign.name);
                        codegen_node(node->data.assign.index, output, level);
                        fprintf(output, ")] = ");
                } else {
                        fprintf(output, "%s = ", node->data.assign.name);
                }
                codegen_node(node->data.assign.value, output, level);
                break;
        default:
                codegen_node(node, output, level);
        }
}

// takes one AST node and writes that node's C code to the file
static void codegen_node(ASTnode *node, FILE *output, int level) {
        switch (node->type) {

        // ---- Literals & identifiers ----
        case NODE_INT_LITERAL:
                fprintf(output, "%d", node->data.int_literal.value);
                break;
        case NODE_FLOAT_LITERAL:
                fprintf(output, "%f", node->data.float_literal.value);
                break;
        case NODE_STRING_LITERAL:
                fprintf(output, "\"%s\"", node->data.string_literal.value);
                break;
        case NODE_BOOL_LITERAL:
                fprintf(output, "%d", node->data.bool_literal.value);
                break;
        case NODE_CHAR_LITERAL:
                fprintf(output, "'%c'", node->data.char_literal.value);
                break;
        case NODE_LIST_LITERAL: {
                // e.g. [1, 2] in a vec<int> decl → __pinum_vec_int_init(2, (int)1, (int)2)
                // each element is cast to the vec's element type so the variadic
                // init() reads the correct C type (e.g. int literals in a vec<double>
                // must be passed as double, not int — otherwise va_arg misreads them).
                const char *vec_type = node->resolved_type ? node->resolved_type : "vec_int32";
                const char *elem = vec_type;
                if (strncmp(vec_type, "vec_", 4) == 0) {
                        elem = codegen_type(vec_type + 4);
                } else {
                        elem = "int32";
                }
                fprintf(output, "__pinum_%s_init(%d", vec_type, node->data.list_literal.count);
                for (int i = 0; i < node->data.list_literal.count; i++) {
                        fprintf(output, ", (%s)", elem);
                        codegen_node(node->data.list_literal.elements[i], output, level);
                }
                fprintf(output, ")");
                break;
        }
        case NODE_IDENTIFIER:
                fprintf(output, "%s", node->data.identifier.name);
                break;
        case NODE_ARRAY_ACCESS:
                // bound check using runtime function first
                fprintf(output, "%s.data[__pinum_check_bounds(%s.size, ", node->data.array_access.name, node->data.array_access.name);
                codegen_node(node->data.array_access.index, output, level);
                fprintf(output, ")]");
                break;

        // ---- Expressions ----
        case NODE_BINARY_EXPRESSION:
                // char repetition
                if (node->data.binary_expression.op == TOKEN_STAR) {
                        if (is_char(node->data.binary_expression.left)) {
                                // 'a' * 3  →  __pinum_repeat_char('a', 3)
                                fprintf(output, "__pinum_repeat_char(");
                                codegen_node(node->data.binary_expression.left, output, level);
                                fprintf(output, ", ");
                                codegen_node(node->data.binary_expression.right, output, level);
                                fprintf(output, ")");
                                break;
                        } else if (is_char(node->data.binary_expression.right)) {
                                // 3 * 'a'  →  __pinum_repeat_char('a', 3)  (args swapped!)
                                fprintf(output, "__pinum_repeat_char(");
                                codegen_node(node->data.binary_expression.right, output, level);
                                fprintf(output, ", ");
                                codegen_node(node->data.binary_expression.left, output, level);
                                fprintf(output, ")");
                                break;
                        }
                }
                // string repetition
                if (node->data.binary_expression.op == TOKEN_STAR) {
                        if (is_string(node->data.binary_expression.left)) {
                                // 'a' * 3  →  __pinum_repeat_char('a', 3)
                                fprintf(output, "__pinum_repeat_string(");
                                codegen_node(node->data.binary_expression.left, output, level);
                                fprintf(output, ", ");
                                codegen_node(node->data.binary_expression.right, output, level);
                                fprintf(output, ")");
                                break;
                        } else if (is_string(node->data.binary_expression.right)) {
                                // 3 * 'a'  →  __pinum_repeat_char('a', 3)  (args swapped!)
                                fprintf(output, "__pinum_repeat_string(");
                                codegen_node(node->data.binary_expression.right, output, level);
                                fprintf(output, ", ");
                                codegen_node(node->data.binary_expression.left, output, level);
                                fprintf(output, ")");
                                break;
                        }
                }
                // string addition
                if (node->data.binary_expression.op == TOKEN_PLUS && is_string(node->data.binary_expression.left) && is_string(node->data.binary_expression.right)) {
                        // "a" + "b"  →  __pinum_add_string__("a", "b")
                        fprintf(output, "__pinum_add_string(");
                        codegen_node(node->data.binary_expression.left, output, level);
                        fprintf(output, ", ");
                        codegen_node(node->data.binary_expression.right, output, level);
                        fprintf(output, ")");
                        break;
                }
                // put in brakets to keep the order
                fprintf(output, "(");
                // left (op) right
                codegen_node(node->data.binary_expression.left, output, level);             // get left value
                fprintf(output, " %s ", codegen_operator(node->data.binary_expression.op)); // print operator
                codegen_node(node->data.binary_expression.right, output, level);            // get right value
                fprintf(output, ")");
                break;
        case NODE_UNARY_EXPRESSION:
                fprintf(output, "(");
                // get operator
                fprintf(output, "%s", codegen_operator(node->data.unary_expression.op));
                codegen_node(node->data.unary_expression.left, output, level);
                fprintf(output, ")");
                break;
        case NODE_TERNARY_EXPRESSION:
                fprintf(output, "(");
                codegen_node(node->data.ternary_expression.condition, output, level);
                fprintf(output, "?");
                codegen_node(node->data.ternary_expression.then_expr, output, level);
                fprintf(output, ":");
                codegen_node(node->data.ternary_expression.else_expr, output, level);
                fprintf(output, ")");
                break;

        // ---- Declarations & assignment ----
        case NODE_VAR_DECL: {
                // build the full C type, e.g. "long int" or "char *" or "vec_int"
                const char *base_type = codegen_decl_type(node);
                char full_type[64];
                if (node->data.var_decl.modifiers) {
                        snprintf(full_type, sizeof(full_type), "%s %s", node->data.var_decl.modifiers, base_type);
                } else {
                        snprintf(full_type, sizeof(full_type), "%s", base_type);
                }
                // get the modifier
                if (node->data.var_decl.modifiers) {
                        fprintf(output, "%s ", node->data.var_decl.modifiers);
                }
                // get type and name
                fprintf(output, "%s %s", base_type, node->data.var_decl.name);
                // check if they have any value assigned
                if (node->data.var_decl.value) {
                        fprintf(output, " = ");
                        codegen_node(node->data.var_decl.value, output, level);
                }
                fprintf(output, ";\n");
                break;
        }
        case NODE_ASSIGN:
                if (node->data.assign.index) {
                        fprintf(output, "%s.data[__pinum_check_bounds(%s.size, ", node->data.assign.name, node->data.assign.name);
                        codegen_node(node->data.assign.index, output, level);
                        fprintf(output, ")] = ");
                } else {
                        fprintf(output, "%s = ", node->data.assign.name);
                }
                codegen_node(node->data.assign.value, output, level);
                fprintf(output, ";\n");
                break;

        // ---- Statements (control flow) ----
        case NODE_BLOCK: {
                fprintf(output, "{\n");
                for (int i = 0; i < node->data.blocks.count; i++) {
                        codegen_stmt(node->data.blocks.statements[i], output, level + 1);
                }
                fprintf(output, "}\n");
                break;
        }
        case NODE_IF_STAT:
                fprintf(output, "if (");
                codegen_node(node->data.if_stat.condition, output, level);
                fprintf(output, ") ");
                codegen_node(node->data.if_stat.then_block, output, level);
                if (node->data.if_stat.else_block) {
                        fprintf(output, " else ");
                        codegen_node(node->data.if_stat.else_block, output, level);
                }
                break;
        case NODE_WHILE:
                fprintf(output, "while (");
                codegen_node(node->data.while_loop.condition, output, level);
                fprintf(output, ") ");
                codegen_node(node->data.while_loop.body, output, level);
                break;
        case NODE_FOR:
                fprintf(output, "for (");
                codegen_for_param(node->data.for_loop.init, output, level);
                fprintf(output, "; ");
                if (node->data.for_loop.condition) {
                        codegen_node(node->data.for_loop.condition, output, level);
                }
                fprintf(output, "; ");
                codegen_for_param(node->data.for_loop.increment, output, level);
                fprintf(output, ")");
                codegen_node(node->data.for_loop.body, output, level);
                break;
        case NODE_RETURN:
                fprintf(output, "return ");
                codegen_node(node->data.returns.expression, output, level);
                fprintf(output, ";\n");
                break;
        case NODE_READ: {
                const char *type = node->resolved_type;
                if (type == NULL) {
                        // can't pick a format specifier without knowing the variable's type
                        fprintf(output, "// TODO: unknown type for read(%s)\n", node->data.read.name);
                        break;
                }
                fprintf(output, "scanf(\"%s\", &%s);\n", specifier_for_type(type), node->data.read.name);
                break;
        }
        case NODE_BREAK:
                fprintf(output, "break;");
                break;
        case NODE_CONTINUE:
                fprintf(output, "continue;");
                break;

        // ---- Built-in statements ----
        case NODE_PRINT: {
                int n = node->data.print.arg_count;
                int group_start = 0;
                for (int i = 0; i < n; i++) {
                        if (is_vec(node->data.print.args[i])) {
                                // flush the scalar group that came before this vec
                                if (i > group_start) {
                                        fprintf(output, "printf(\"");
                                        for (int j = group_start; j < i; j++) {
                                                fprintf(output, "%s", codegen_specifier(node->data.print.args[j]));
                                        }
                                        fprintf(output, "\"");
                                        for (int j = group_start; j < i; j++) {
                                                fprintf(output, ", ");
                                                codegen_node(node->data.print.args[j], output, level);
                                        }
                                        fprintf(output, ");\n");
                                }
                                // vec args print their own "[1, 2]" via the runtime helper
                                fprintf(output, "__pinum_%s_print(", node->data.print.args[i]->resolved_type);
                                codegen_node(node->data.print.args[i], output, level);
                                fprintf(output, ");\n");
                                group_start = i + 1;
                        }
                }
                // flush any remaining scalar args
                if (group_start < n) {
                        fprintf(output, "printf(\"");
                        for (int j = group_start; j < n; j++) {
                                fprintf(output, "%s", codegen_specifier(node->data.print.args[j]));
                        }
                        fprintf(output, "\"");
                        for (int j = group_start; j < n; j++) {
                                fprintf(output, ", ");
                                codegen_node(node->data.print.args[j], output, level);
                        }
                        fprintf(output, ");\n");
                }
                if (node->data.print.newline) {
                        fprintf(output, "printf(\"\\n\");\n");
                }
                break;
        }

        // ---- Functions ----
        case NODE_FUNC_DEF: {
                // 'main' is the program entry point → emit as C's int main(void)
                if (strcmp(node->data.func_def.name, "main") == 0) {
                        fprintf(output, "int main(void) ");
                        codegen_node(node->data.func_def.body, output, level); // body prints { ... }
                        break;
                }

                // return type void if no return type
                const char *ret = codegen_type(node->data.func_def.return_type ? node->data.func_def.return_type : "void");
                fprintf(output, "%s %s(", ret, node->data.func_def.name);
                for (int i = 0; i < node->data.func_def.param_count; i++) {
                        if (i) fprintf(output, ", ");
                        // each param is a NODE_VAR_DECL; codegen_for_param prints "type name" (no ';')
                        codegen_for_param(node->data.func_def.params[i], output, level);
                }
                fprintf(output, ") ");
                codegen_node(node->data.func_def.body, output, level); // NODE_BLOCK prints { ... }
                break;
        }
        case NODE_FUNC_CALL: {
                fprintf(output, "%s(", node->data.func_call.name);
                for (int i = 0; i < node->data.func_call.arg_count; i++) {
                        if (i) fprintf(output, ", ");
                        codegen_node(node->data.func_call.args[i], output, level);
                }
                fprintf(output, ")");
                break;
        }

        // ---- Member access & method calls ----
        case NODE_MEMBER_ACCESS: {
                ASTnode *obj = node->data.member_access.object;
                const char *member = node->data.member_access.member;
                if (node->data.member_access.arg_count > 0) {
                        const method_def *m = method_lookup(obj->resolved_type, member);
                        // build helper name: __pinum_%_append
                        char fn[64];
                        snprintf(fn, sizeof(fn), m->c_helper, obj->resolved_type);
                        fprintf(output, "%s(&", fn);
                        codegen_node(obj, output, level);
                        fprintf(output, ", ");
                        codegen_node(node->data.member_access.args[0], output, level);
                        fprintf(output, ");\n");
                } else {
                        // property read: obj.member
                        codegen_node(obj, output, level);
                        fprintf(output, ".%s", member);
                }
                break;
        }

        // ---- Directives & other ----
        case NODE_DIRECTIVE:
                break; // TODO
        case NODE_IMPORT:
                break; // TODO

        default:
                fprintf(output, "//TODO: %s\n", node_type_name(node->type));
        }
}

// --- MAIN ---
void codegen_c(ASTnode *program, FILE *output) {
#ifndef __wasm__
        char *home_dir = getenv("HOME");
        char path[1024];
        snprintf(path, sizeof(path), "%s/.pinum-lang/runtime/pinum_runtime.h", home_dir);

        // check for .pinum-lang directory
        if (access(path, F_OK)) {
                pinum_error(STAGE_CODEGEN, ERR_RUNTIME_MISSING, path);
        }

        fprintf(output, "#include \"%s/.pinum-lang/runtime/pinum_runtime.h\"\n", home_dir);
#else
        fprintf(output, "#include \"pinum_runtime.h\"\n");
        fprintf(output, "#include \"pinum_rtvec.h\"\n");
        fprintf(output, "#include \"pinum_rtstrchr.h\"\n");
#endif
        // emit every top-level declaration at file scope
        for (int i = 0; i < program->data.program.count; i++) {
                ASTnode *stmt = program->data.program.statements[i];
                if (stmt->type == NODE_FUNC_DEF || stmt->type == NODE_VAR_DECL) {
                        codegen_node(stmt, output, 0);
                }
                // sema already rejected any other top-level statement
        }
}

// wraps codegen_node with a trailing ';' for expression statements
// that don't self-terminate (currently only NODE_FUNC_CALL).
static void codegen_stmt(ASTnode *node, FILE *output, int level) {
        codegen_node(node, output, level);
        if (node && node->type == NODE_FUNC_CALL) {
                fprintf(output, ";\n");
        }
}
