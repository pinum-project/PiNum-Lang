/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

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
        return "%d";
}

// maps a Quil type name to its C equivalent
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

// maps a declaration to its concrete C type
static const char *codegen_decl_type(ASTnode *node) {
        return codegen_type(node->data.var_decl.type_name);
}

// maps a Quil operator token to its C equivalent
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
                const char *base_type = codegen_decl_type(node);
                if (node->data.var_decl.modifiers) {
                        fprintf(output, "%s ", node->data.var_decl.modifiers);
                }
                fprintf(output, "%s %s", base_type, node->data.var_decl.name);
                if (node->data.var_decl.is_array) {
                        fprintf(output, "[%d]", node->data.var_decl.array_size);
                }
                if (node->data.var_decl.value) {
                        fprintf(output, " = ");
                        codegen_node(node->data.var_decl.value, output, level);
                }
                break;
        }
        case NODE_ASSIGN:
                if (node->data.assign.index) {
                        fprintf(output, "%s[", node->data.assign.name);
                        codegen_node(node->data.assign.index, output, level);
                        fprintf(output, "] = ");
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
                // array initializer: [1, 2, 3] -> {1, 2, 3}
                fprintf(output, "{");
                for (int i = 0; i < node->data.list_literal.count; i++) {
                        if (i) fprintf(output, ", ");
                        codegen_node(node->data.list_literal.elements[i], output, level);
                }
                fprintf(output, "}");
                break;
        }
        case NODE_IDENTIFIER:
                fprintf(output, "%s", node->data.identifier.name);
                break;
        case NODE_ARRAY_ACCESS:
                fprintf(output, "%s[", node->data.array_access.name);
                codegen_node(node->data.array_access.index, output, level);
                fprintf(output, "]");
                break;

        // ---- Expressions ----
        case NODE_BINARY_EXPRESSION:
                // char repetition
                if (node->data.binary_expression.op == TOKEN_STAR) {
                        if (is_char(node->data.binary_expression.left)) {
                                // 'a' * 3  →  __quil_repeat_char('a', 3)
                                fprintf(output, "__quil_repeat_char(");
                                codegen_node(node->data.binary_expression.left, output, level);
                                fprintf(output, ", ");
                                codegen_node(node->data.binary_expression.right, output, level);
                                fprintf(output, ")");
                                break;
                        } else if (is_char(node->data.binary_expression.right)) {
                                // 3 * 'a'  →  __quil_repeat_char('a', 3)  (args swapped!)
                                fprintf(output, "__quil_repeat_char(");
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
                                // 'a' * 3  →  __quil_repeat_char('a', 3)
                                fprintf(output, "__quil_repeat_string(");
                                codegen_node(node->data.binary_expression.left, output, level);
                                fprintf(output, ", ");
                                codegen_node(node->data.binary_expression.right, output, level);
                                fprintf(output, ")");
                                break;
                        } else if (is_string(node->data.binary_expression.right)) {
                                // 3 * 'a'  →  __quil_repeat_char('a', 3)  (args swapped!)
                                fprintf(output, "__quil_repeat_string(");
                                codegen_node(node->data.binary_expression.right, output, level);
                                fprintf(output, ", ");
                                codegen_node(node->data.binary_expression.left, output, level);
                                fprintf(output, ")");
                                break;
                        }
                }
                // string addition
                if (node->data.binary_expression.op == TOKEN_PLUS && is_string(node->data.binary_expression.left) && is_string(node->data.binary_expression.right)) {
                        // "a" + "b"  →  __quil_add_string__("a", "b")
                        fprintf(output, "__quil_add_string(");
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
                const char *base_type = codegen_decl_type(node);
                if (node->data.var_decl.modifiers) {
                        fprintf(output, "%s ", node->data.var_decl.modifiers);
                }
                fprintf(output, "%s %s", base_type, node->data.var_decl.name);
                if (node->data.var_decl.is_array) {
                        fprintf(output, "[%d]", node->data.var_decl.array_size);
                }
                if (node->data.var_decl.value) {
                        fprintf(output, " = ");
                        codegen_node(node->data.var_decl.value, output, level);
                }
                fprintf(output, ";\n");
                break;
        }
        case NODE_ASSIGN:
                if (node->data.assign.index) {
                        fprintf(output, "%s[", node->data.assign.name);
                        codegen_node(node->data.assign.index, output, level);
                        fprintf(output, "] = ");
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
                if (n > 0) {
                        fprintf(output, "printf(\"");
                        for (int j = 0; j < n; j++) {
                                fprintf(output, "%s", codegen_specifier(node->data.print.args[j]));
                        }
                        fprintf(output, "\"");
                        for (int j = 0; j < n; j++) {
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

        // ---- Member access ----
        case NODE_MEMBER_ACCESS: {
                ASTnode *obj = node->data.member_access.object;
                const char *member = node->data.member_access.member;
                if (node->data.member_access.arg_count > 0) {
                        fprintf(output, "// TODO: method call %s.%s\n", obj->data.identifier.name, member);
                } else {
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
        char *home_dir = getenv("HOME");
        char path[1024];
        snprintf(path, sizeof(path), "%s/.quil-lang/runtime/quil_runtime.h", home_dir);

        // check for .quil-lang directory
        if (access(path, F_OK)) {
                quil_error(STAGE_CODEGEN, ERR_RUNTIME_MISSING, path);
        }

        fprintf(output, "#include \"%s/.quil-lang/runtime/quil_runtime.h\"\n", home_dir);
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
