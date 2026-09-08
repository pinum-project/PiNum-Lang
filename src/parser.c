/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

// NOTE: This parser uses recursive decent parsing method.
#include "../include/parser.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// --- initialization function (main) ---
ASTnode *quil_parse(token_list *tokens) {
        Parser parser = {tokens, 0};
        return parse_program(&parser);
}
ASTnode *parse_line(token_list *tokens) {
        Parser parser = {tokens, 0};
        return parse_statement(&parser);
}

// --- Internal parsing functions (using recursive decent) ---
ASTnode *parse_program(Parser *parser) {
        ASTnode *program = create_ast_node(NODE_PROGRAM);
        while (!check(parser, TOKEN_EOF)) {
                // Skip newlines between statements
                if (match(parser, TOKEN_NLINE)) {
                        continue;
                }
                if (match(parser, TOKEN_FN)) {
                        ast_add_statement(program, parse_func_def(parser));
                        continue;
                }
                ast_add_statement(program, parse_statement(parser));
        }
        return program;
}
// - statement level parsing -
ASTnode *parse_statement(Parser *parser) {
        if (check(parser, TOKEN_FN)) {
                advance(parser); // consume 'fn'
                return parse_func_def(parser);
        }
        if (check(parser, TOKEN_INT8) || check(parser, TOKEN_INT16) ||
            check(parser, TOKEN_INT32) || check(parser, TOKEN_INT64) ||
            check(parser, TOKEN_UINT8) || check(parser, TOKEN_UINT16) ||
            check(parser, TOKEN_UINT32) || check(parser, TOKEN_UINT64) ||
            check(parser, TOKEN_FLOAT32) || check(parser, TOKEN_FLOAT64) ||
            check(parser, TOKEN_CHAR) || check(parser, TOKEN_STRING) ||
            check(parser, TOKEN_BOOL) || check(parser, TOKEN_VEC)) {
                return parse_declaration(parser);
        }
        if (match(parser, TOKEN_PRINT)) {
                token print_token = parser->tokens->tokens[parser->current - 1];
                consume(parser, TOKEN_LRPAREN, "'(' after print");
                ASTnode *node = make_print_node();
                ast_set_loc(node, print_token.line, print_token.col);
                if (!check(parser, TOKEN_RRPAREN)) {
                        do {
                                ast_add_print_arg(node, parse_expression(parser));
                        } while (match(parser, TOKEN_COMMA));
                }
                consume(parser, TOKEN_RRPAREN, "')' after arguments");
                consume_end_of_statement(parser);
                return node;
        }
        if (match(parser, TOKEN_PRINTLN)) {
                token println_token = parser->tokens->tokens[parser->current - 1];
                consume(parser, TOKEN_LRPAREN, "'(' after println");
                ASTnode *node = make_print_node();
                node->data.print.newline = true;
                ast_set_loc(node, println_token.line, println_token.col);
                if (!check(parser, TOKEN_RRPAREN)) {
                        do {
                                ast_add_print_arg(node, parse_expression(parser));
                        } while (match(parser, TOKEN_COMMA));
                }
                consume(parser, TOKEN_RRPAREN, "')' after arguments");
                consume_end_of_statement(parser);
                return node;
        }
        if (match(parser, TOKEN_READ)) {
                return parse_read_statement(parser);
        }
        if (match(parser, TOKEN_IF)) {
                return parse_if_statement(parser);
        }
        if (match(parser, TOKEN_WHILE)) {
                return parse_while_statement(parser);
        }
        if (match(parser, TOKEN_FOR)) {
                return parse_for_statement(parser);
        }
        if (match(parser, TOKEN_RETURN)) {
                return parse_return_statement(parser);
        }
        if (match(parser, TOKEN_BREAK)) {
                token kw = parser->tokens->tokens[parser->current - 1];
                consume_end_of_statement(parser);
                ASTnode *node = create_ast_node(NODE_BREAK);
                ast_set_loc(node, kw.line, kw.col);
                return node;
        }
        if (match(parser, TOKEN_CONTINUE)) {
                token kw = parser->tokens->tokens[parser->current - 1];
                consume_end_of_statement(parser);
                ASTnode *node = create_ast_node(NODE_CONTINUE);
                ast_set_loc(node, kw.line, kw.col);
                return node;
        }
        if (match(parser, TOKEN_SCOPE)) {
                token ns = consume(parser, TOKEN_ID, "namespace name after 'scope'");
                ASTnode *body = parse_block(parser); // { ... }
                return make_namespace_node(ns.value, body);
        }
        if (match(parser, TOKEN_ATSIGN)) {
                // The lexer splits '@import' into TOKEN_ATSIGN and TOKEN_IMPORT
                token atsign_tok = parser->tokens->tokens[parser->current - 1];
                token name_token = advance(parser);
                char *name = name_token.value;
                char *value = NULL;
                // Allow IDs or LIB tokens
                if (check(parser, TOKEN_ID) || check(parser, TOKEN_LIB_STDLIB)) {
                        value = advance(parser).value;
                }
                ASTnode *node = make_directive_node(name, value);
                ast_set_loc(node, atsign_tok.line, atsign_tok.col);
                return node;
        }

        // Skip newlines before statement
        if (match(parser, TOKEN_NLINE)) {
                // If it is a newline, we have already consumed it with match.
                // Just return NULL or continue to next statement.
                return NULL;
        }

        // Fallback to expression statement
        ASTnode *expression = parse_expression(parser);
        consume_end_of_statement(parser);
        return expression;

        // TODO: add more statements to be parsed
}
ASTnode *parse_if_statement(Parser *parser) {
        token kw = parser->tokens->tokens[parser->current - 1]; // 'if' keyword
        // Parsing the expresseion inside ( )
        consume(parser, TOKEN_LRPAREN, "'('");
        ASTnode *condition = parse_expression(parser);
        consume(parser, TOKEN_RRPAREN, "')'");

        ASTnode *then_block = parse_block(parser);
        ASTnode *else_block = NULL;
        if (match(parser, TOKEN_ELSE)) {
                // check for 'else if'
                if (match(parser, TOKEN_IF)) {
                        else_block = parse_if_statement(parser);
                } else {
                        else_block = parse_block(parser);
                }
        }

        ASTnode *node = make_if_stat_node(condition, then_block, else_block);
        ast_set_loc(node, kw.line, kw.col);
        return node;
}
ASTnode *parse_while_statement(Parser *parser) {
        token kw = parser->tokens->tokens[parser->current - 1]; // 'while' keyword
        // parsing the expression inside ( )
        consume(parser, TOKEN_LRPAREN, "'('");
        ASTnode *condition = parse_expression(parser);
        consume(parser, TOKEN_RRPAREN, "')'");
        ASTnode *body = parse_block(parser);

        ASTnode *node = make_while_node(condition, body);
        ast_set_loc(node, kw.line, kw.col);
        return node;
}
// for(init; condition; inc) {body}
ASTnode *parse_for_statement(Parser *parser) {
        token kw = parser->tokens->tokens[parser->current - 1]; // 'for' keyword
        consume(parser, TOKEN_LRPAREN, "'('");

        // init
        ASTnode *init = NULL;
        // for (int i = 0; i < n; i++) {body}
        if (check(parser, TOKEN_INT8) || check(parser, TOKEN_INT16) ||
            check(parser, TOKEN_INT32) || check(parser, TOKEN_INT64) ||
            check(parser, TOKEN_UINT8) || check(parser, TOKEN_UINT16) ||
            check(parser, TOKEN_UINT32) || check(parser, TOKEN_UINT64) ||
            check(parser, TOKEN_FLOAT32) || check(parser, TOKEN_FLOAT64) ||
            check(parser, TOKEN_CHAR) || check(parser, TOKEN_STRING) ||
            check(parser, TOKEN_BOOL) || check(parser, TOKEN_VEC)) {
                init = parse_declaration(parser);
        } else if (check(parser, TOKEN_SEMICOLON)) {
                advance(parser); // skip empty init ';'
        } else {
                // for (range) {body}  OR  for (i = 0; cond; inc) {body}
                ASTnode *range = parse_expression(parser);
                // for (range)  →  for (int __quil_i<N> = 0; __quil_i<N> < RANGE; __quil_i<N>++)
                if (check(parser, TOKEN_RRPAREN)) {
                        consume(parser, TOKEN_RRPAREN, "')'");
                        ASTnode *body = parse_block(parser);

                        // generate uniques index name for each scope
                        static int rng_counter = 0;
                        const char *idx = "__quil_i";
                        char idx_name[32];
                        snprintf(idx_name, sizeof(idx_name), "%s%d", idx, rng_counter++);
                        ASTnode *rinit = make_var_decl_node("int", NULL, (char *)idx_name, make_int_node(0), false, 0);
                        ASTnode *cond = make_binary_node(make_identifier_node((char *)idx_name), TOKEN_LABRACKET, range);
                        ASTnode *inc = make_assign_node((char *)idx_name,
                                                        make_binary_node(make_identifier_node((char *)idx_name), TOKEN_PLUS, make_int_node(1)));
                        return make_for_node(rinit, cond, inc, body);
                }
                consume(parser, TOKEN_SEMICOLON, "';'");
                init = range;
        }
        // condition
        ASTnode *condition = NULL;
        if (!check(parser, TOKEN_SEMICOLON)) {
                condition = parse_expression(parser);
        }
        consume(parser, TOKEN_SEMICOLON, "';'");
        // inc
        ASTnode *inc = NULL;
        if (!check(parser, TOKEN_RRPAREN)) {
                inc = parse_expression(parser);
        }
        consume(parser, TOKEN_RRPAREN, "')'");

        ASTnode *body = parse_block(parser);
        ASTnode *node = make_for_node(init, condition, inc, body);
        ast_set_loc(node, kw.line, kw.col);
        return node;
}
ASTnode *parse_return_statement(Parser *parser) {
        token kw = parser->tokens->tokens[parser->current - 1]; // 'return' keyword
        ASTnode *expression = parse_expression(parser);
        consume_end_of_statement(parser);
        ASTnode *node = create_ast_node(NODE_RETURN);
        node->data.returns.expression = expression;
        ast_set_loc(node, kw.line, kw.col);
        return node;
}
ASTnode *parse_read_statement(Parser *parser) {
        consume(parser, TOKEN_LRPAREN, "'(' after read");
        token name_token = consume(parser, TOKEN_ID, "a variable name");
        consume(parser, TOKEN_RRPAREN, "')' after variable name");
        consume_end_of_statement(parser);
        ASTnode *node = make_read_node(name_token.value);
        ast_set_loc(node, name_token.line, name_token.col);
        return node;
}
// parse a type: simple (int32, uint8, etc.)
static void parse_type(Parser *parser, char **out_type_name, char **out_element_type) {
        *out_type_name = NULL;
        *out_element_type = NULL;

        if (check(parser, TOKEN_VEC)) {
                token t = peek(parser);
                quil_error_at(STAGE_PARSER, ERR_UNKNOWN, t.line, t.col, "vec has been removed, use array type 'int32[N]' (vec will be stdlib later)");
        }
        if (check(parser, TOKEN_ID) && strcmp(peek(parser).value, "void") == 0) {
                advance(parser);
                *out_type_name = "void";
        } else if (match(parser, TOKEN_INT8)) *out_type_name = "int8";
        else if (match(parser, TOKEN_INT16)) *out_type_name = "int16";
        else if (match(parser, TOKEN_INT32)) *out_type_name = "int32";
        else if (match(parser, TOKEN_INT64)) *out_type_name = "int64";
        else if (match(parser, TOKEN_UINT8)) *out_type_name = "uint8";
        else if (match(parser, TOKEN_UINT16)) *out_type_name = "uint16";
        else if (match(parser, TOKEN_UINT32)) *out_type_name = "uint32";
        else if (match(parser, TOKEN_UINT64)) *out_type_name = "uint64";
        else if (match(parser, TOKEN_FLOAT32)) *out_type_name = "float32";
        else if (match(parser, TOKEN_FLOAT64)) *out_type_name = "float64";
        else if (match(parser, TOKEN_CHAR)) *out_type_name = "char";
        else if (match(parser, TOKEN_STRING)) *out_type_name = "string";
        else if (match(parser, TOKEN_BOOL)) *out_type_name = "bool";
        else {
                token found = peek(parser);
                quil_expected_at(STAGE_PARSER, found.line, found.col, "a data type (int32, float64, etc.)", peek_display(parser));
        }
}
ASTnode *parse_declaration(Parser *parser) {
        char *data_type = NULL;
        char *element_type = NULL;

        // Data Type (Required)
        parse_type(parser, &data_type, &element_type);

        // array suffix: int32[512]
        bool is_array = false;
        int array_size = 0;
        if (match(parser, TOKEN_LSPAREN)) {
                token sz = consume(parser, TOKEN_INUM, "array size");
                array_size = sz.int_value;
                consume(parser, TOKEN_RSPAREN, "']' after array size");
                is_array = true;
        }

        // Veriable name
        token name_token = consume(parser, TOKEN_ID, "a variable name");
        char *var_name = name_token.value; // value is actually the name of the token

        ASTnode *initializer = NULL;
        if (match(parser, TOKEN_EQUAL)) {
                initializer = parse_expression(parser);
        }
        // expecting for a semicolon at the end
        consume_end_of_statement(parser);

        ASTnode *decl = make_var_decl_node(data_type, NULL, var_name, initializer, is_array, array_size);
        ast_set_loc(decl, name_token.line, name_token.col);
        return decl;
}
ASTnode *parse_block(Parser *parser) {
        consume(parser, TOKEN_LCPAREN, "'{'");
        ASTnode *block = create_ast_node(NODE_BLOCK);

        while (!check(parser, TOKEN_RCPAREN) && !check(parser, TOKEN_EOF)) {
                if (match(parser, TOKEN_NLINE)) continue;
                ast_add_statement(block, parse_statement(parser));
        }

        consume(parser, TOKEN_RCPAREN, "'}'");
        return block;
}

// - Function parsing -
ASTnode *parse_func_def_param(Parser *parser) {
        char *type_name = NULL;
        char *element_type = NULL;
        parse_type(parser, &type_name, &element_type);

        bool is_array = false;
        int array_size = 0;
        if (match(parser, TOKEN_LSPAREN)) {
                token sz = consume(parser, TOKEN_INUM, "array size");
                array_size = sz.int_value;
                consume(parser, TOKEN_RSPAREN, "']' after array size");
                is_array = true;
        }

        token name_token = consume(parser, TOKEN_ID, "a parameter name");
        ASTnode *param = make_var_decl_node(type_name, NULL, name_token.value, NULL, is_array, array_size);
        ast_set_loc(param, name_token.line, name_token.col);
        return param;
}
ASTnode *parse_func_def(Parser *parser) {
        token func_name = consume(parser, TOKEN_ID, "function name");
        consume(parser, TOKEN_LRPAREN, "'(' after function name");

        // add parameters to the parameter list
        ASTnode **params = NULL;
        int param_count = 0;
        int param_capacity = 0;
        if (!check(parser, TOKEN_RRPAREN)) {
                do {
                        if (param_count >= param_capacity) {
                                param_capacity = param_capacity == 0 ? 4 : param_capacity * 2;
                                params = realloc(params, sizeof(ASTnode *) * param_capacity);
                        }
                        params[param_count++] = parse_func_def_param(parser);
                } while (match(parser, TOKEN_COMMA));
        }
        consume(parser, TOKEN_RRPAREN, "')' after function parameters");

        char *return_type = NULL;
        char *ret_element = NULL;
        // optional '->' return type; omitting it means void
        if (check(parser, TOKEN_ARROW)) {
                advance(parser); // consume '->'
                parse_type(parser, &return_type, &ret_element);
        } else {
                return_type = "void";
        }

        ASTnode *body = parse_block(parser);
        ASTnode *def = make_func_def_node(return_type, func_name.value, params, param_count, body);
        return def;
}

// - Expression parsing -
// Takes the parsed binary node and makes a specific node
ASTnode *parse_primary(Parser *parser) {
        if (match(parser, TOKEN_INUM)) {
                token tok = parser->tokens->tokens[parser->current - 1];
                ASTnode *node = make_int_node(tok.int_value);
                ast_set_loc(node, tok.line, tok.col);
                return node;
        }
        if (match(parser, TOKEN_FNUM)) {
                token tok = parser->tokens->tokens[parser->current - 1];
                ASTnode *node = make_float_node(tok.float_value);
                ast_set_loc(node, tok.line, tok.col);
                return node;
        }
        if (match(parser, TOKEN_ID)) {
                token tok = parser->tokens->tokens[parser->current - 1];
                ASTnode *node = make_identifier_node(tok.value);
                ast_set_loc(node, tok.line, tok.col);
                return node;
        }
        if (match(parser, TOKEN_TRUE)) {
                token tok = parser->tokens->tokens[parser->current - 1];
                ASTnode *node = make_bool_node(true);
                ast_set_loc(node, tok.line, tok.col);
                return node;
        }
        if (match(parser, TOKEN_FALSE)) {
                token tok = parser->tokens->tokens[parser->current - 1];
                ASTnode *node = make_bool_node(false);
                ast_set_loc(node, tok.line, tok.col);
                return node;
        }

        if (match(parser, TOKEN_LRPAREN)) {
                ASTnode *expr = parse_expression(parser);
                consume(parser, TOKEN_RRPAREN, "')' after expression");
                return expr;
        }

        if (match(parser, TOKEN_DQUOTE) || match(parser, TOKEN_SQUOTE)) {
                token quote_tok = parser->tokens->tokens[parser->current - 1];
                tokenType quote = quote_tok.type;
                // Collect string content
                char *str_content = strdup("");
                int content_tokens = 0;
                while (!check(parser, TOKEN_DQUOTE) && !check(parser, TOKEN_SQUOTE) && !check(parser, TOKEN_EOF)) {
                        token t = advance(parser);
                        content_tokens++;
                        if (t.value) {
                                char *new_str = malloc(strlen(str_content) + strlen(t.value) + 1);
                                sprintf(new_str, "%s%s", str_content, t.value);
                                free(str_content);
                                str_content = new_str;
                        }
                }
                advance(parser); // consume closing quote
                // a single-quoted literal with exactly one 1-character token is a char literal
                if (quote == TOKEN_SQUOTE && content_tokens == 1 && str_content[0] != '\0' && str_content[1] == '\0') {
                        char c = str_content[0];
                        free(str_content);
                        ASTnode *charnode = make_char_node(c);
                        ast_set_loc(charnode, quote_tok.line, quote_tok.col);
                        return charnode;
                }
                ASTnode *node = make_string_node(str_content);
                free(str_content);
                ast_set_loc(node, quote_tok.line, quote_tok.col);
                return node;
        }

        if (match(parser, TOKEN_LSPAREN)) {
                token bracket_tok = parser->tokens->tokens[parser->current - 1];
                ASTnode **elements = NULL;
                int count = 0;
                int capacity = 0;
                if (!check(parser, TOKEN_RSPAREN)) {
                        do {
                                ASTnode *elem = parse_expression(parser);
                                if (count >= capacity) {
                                        capacity = capacity == 0 ? 4 : capacity * 2;
                                        elements = realloc(elements, sizeof(ASTnode *) * capacity);
                                }
                                elements[count++] = elem;
                        } while (match(parser, TOKEN_COMMA));
                }
                consume(parser, TOKEN_RSPAREN, "']' after vec elements");
                ASTnode *list_node = make_list_literal_node(elements, count);
                ast_set_loc(list_node, bracket_tok.line, bracket_tok.col);
                return list_node;
        }

        // Error
        token found = peek(parser);
        quil_error_at(STAGE_PARSER, ERR_UNEXPECTED_TOKEN, found.line, found.col, peek_display(parser));
}
// parses function call and member access
// parses and refers to parse_primary
ASTnode *parse_call(Parser *parser) {
        ASTnode *node = parse_primary(parser);
        // fold a::b::c into NODE_QUALIFIED (must be before call/dot handling)
        if (node->type == NODE_IDENTIFIER) {
                int cap = 4, count = 1;
                char **segs = malloc(cap * sizeof(char *));
                segs[0] = strdup(node->data.identifier.name);
                int qline = node->line, qcol = node->col;
                while (match(parser, TOKEN_DCOLON)) {
                        token t = consume(parser, TOKEN_ID, "identifier after '::'");
                        if (count >= cap) {
                                cap *= 2;
                                segs = realloc(segs, cap * sizeof(char *));
                        }
                        segs[count++] = strdup(t.value);
                }
                if (count > 1) {
                        free_ast_node(node);
                        node = make_qualified_node(segs, count);
                        ast_set_loc(node, qline, qcol);
                        // qualified path must be a function call: require '(' after '::' chain
                        if (!check(parser, TOKEN_LRPAREN)) {
                                quil_error_at(STAGE_PARSER, ERR_INVALID_CALL_TARGET, qline, qcol, "qualified path 'std::foo' must be called as 'std::foo()'");
                        }
                } else {
                        free(segs[0]);
                        free(segs);
                }
        }

        while (true) {
                // function call parsing
                // function call identified by NODE_IDENTIFIER or NODE_QUALIFIED followed by TOKEN_LRPAREN
                if (match(parser, TOKEN_LRPAREN)) {
                        if (node->type != NODE_IDENTIFIER && node->type != NODE_QUALIFIED) {
                                token trigger = parser->tokens->tokens[parser->current - 1];
                                quil_error_at(STAGE_PARSER, ERR_INVALID_CALL_TARGET, trigger.line, trigger.col, node_type_name(node->type));
                        }
                        int line = node->line;
                        int col = node->col;
                        char *name;
                        if (node->type == NODE_QUALIFIED) {
                                // join segments with "::" for func_call.name
                                size_t len = 0;
                                for (int i = 0; i < node->data.qualified.count; i++) len += strlen(node->data.qualified.segments[i]) + 2;
                                name = malloc(len + 1);
                                name[0] = '\0';
                                for (int i = 0; i < node->data.qualified.count; i++) {
                                        if (i) strcat(name, "::");
                                        strcat(name, node->data.qualified.segments[i]);
                                }
                        } else {
                                name = strdup(node->data.identifier.name);
                        }
                        free_ast_node(node);
                        ASTnode *call = make_func_call_node(name, NULL, 0);
                        ast_set_loc(call, line, col);
                        free(name);
                        // if the next token is not ')' TOKEN_RRPAREN, there must be args
                        if (!check(parser, TOKEN_RRPAREN)) {
                                do {
                                        ast_add_arg(call, parse_expression(parser));
                                } while (match(parser, TOKEN_COMMA));
                        }
                        consume(parser, TOKEN_RRPAREN, "')' after arguments");
                        node = call;
                }
                // array access
                // array access if NODE_IDENTIFIER followed by TOKEN_LSPAREN
                else if (match(parser, TOKEN_LSPAREN)) {
                        ASTnode *index = parse_expression(parser);
                        consume(parser, TOKEN_RSPAREN, "']' after index");
                        ASTnode *access = create_ast_node(NODE_ARRAY_ACCESS);
                        access->data.array_access.name = node->data.identifier.name;
                        access->data.array_access.index = index;
                        ast_set_loc(access, node->line, node->col);
                        node = access;
                }
                // member access / method call
                // obj.member  or  obj.method(args)
                else if (match(parser, TOKEN_DOT)) {
                        token m = consume(parser, TOKEN_ID, "a member name after '.'");
                        ASTnode **args = NULL;
                        int arg_count = 0;
                        if (match(parser, TOKEN_LRPAREN)) {
                                if (!check(parser, TOKEN_RRPAREN)) {
                                        do {
                                                ast_add_member_arg(&args, &arg_count, parse_expression(parser));
                                        } while (match(parser, TOKEN_COMMA));
                                }
                                consume(parser, TOKEN_RRPAREN, "')' after arguments");
                        }
                        node = make_member_access_node(node, m.value, args, arg_count);
                        ast_set_loc(node, m.line, m.col);
                }
                // postfix increment
                // i++  →  i = (i + 1)
                else if (match(parser, TOKEN_PPLUS)) {
                        if (node->type != NODE_IDENTIFIER) {
                                token trigger = parser->tokens->tokens[parser->current - 1];
                                quil_error_at(STAGE_PARSER, ERR_INVALID_ASSIGN_TARGET, trigger.line, trigger.col, node_type_name(node->type));
                        }
                        int line = node->line;
                        int col = node->col;
                        char *name = strdup(node->data.identifier.name);
                        free_ast_node(node);
                        ASTnode *inc = make_binary_node(make_identifier_node(name), TOKEN_PLUS, make_int_node(1));
                        node = make_assign_node(name, inc);
                        ast_set_loc(node, line, col);
                        free(name);
                }
                // postfix decrement
                // i--  →  i = (i - 1)
                else if (match(parser, TOKEN_MMINUS)) {
                        if (node->type != NODE_IDENTIFIER) {
                                token trigger = parser->tokens->tokens[parser->current - 1];
                                quil_error_at(STAGE_PARSER, ERR_INVALID_ASSIGN_TARGET, trigger.line, trigger.col, node_type_name(node->type));
                        }
                        int line = node->line;
                        int col = node->col;
                        char *name = strdup(node->data.identifier.name);
                        free_ast_node(node);
                        ASTnode *dec = make_binary_node(make_identifier_node(name), TOKEN_MINUS, make_int_node(1));
                        node = make_assign_node(name, dec);
                        ast_set_loc(node, line, col);
                        free(name);
                } else {
                        break;
                }
        }
        return node;
}
// parsing ! and - in front of values
// parses and refers to parse_call
ASTnode *parse_unary(Parser *parser) {
        if (match(parser, TOKEN_EXCLAMATION) || match(parser, TOKEN_MINUS)) {
                token op_tok = parser->tokens->tokens[parser->current - 1];
                tokenType op = op_tok.type;
                ASTnode *left = parse_unary(parser);
                ASTnode *node = make_unary_node(op, left);
                ast_set_loc(node, op_tok.line, op_tok.col);
                return node;
        }
        return parse_call(parser);
}
// parsing * (multiplication), / (division) and % (modulo)
// parses and refers to parse_unary
ASTnode *parse_factors(Parser *parser) {
        ASTnode *node = parse_unary(parser);

        // TOKEN_STAR may be used for multiplication, TOKEN_FSLASH may be used for division and TOKEN_PERCENT may be used for modulo
        while (match(parser, TOKEN_STAR) || match(parser, TOKEN_FSLASH) || match(parser, TOKEN_PERCENT)) {
                token op_tok = parser->tokens->tokens[parser->current - 1];
                tokenType op = op_tok.type;
                ASTnode *right = parse_call(parser);
                node = make_binary_node(node, op, right);
                ast_set_loc(node, op_tok.line, op_tok.col);
        }
        return node;
}
// parsing + (addidtion) and - (subtraction)
// parses and refers to parse_factors
ASTnode *parse_term(Parser *parser) {
        ASTnode *node = parse_factors(parser);

        while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
                token op_tok = parser->tokens->tokens[parser->current - 1];
                tokenType op = op_tok.type;
                ASTnode *right = parse_factors(parser);
                node = make_binary_node(node, op, right);
                ast_set_loc(node, op_tok.line, op_tok.col);
        }
        return node;
}
// parsing < (lesser than), > (greater than), <= (less than equal to) and >= (more than equal to)
// parses and refers to parse_term
ASTnode *parse_comparison(Parser *parser) {
        ASTnode *node = parse_term(parser);

        while (match(parser, TOKEN_LABRACKET) || match(parser, TOKEN_RABRACKET) ||
               match(parser, TOKEN_LEQUAL) || match(parser, TOKEN_GEQUAL)) {
                token op_tok = parser->tokens->tokens[parser->current - 1];
                tokenType op = op_tok.type;
                ASTnode *right = parse_term(parser);
                node = make_binary_node(node, op, right);
                ast_set_loc(node, op_tok.line, op_tok.col);
        }
        return node;
}
// parsing == (equal to) and != (not equal to)
// parses and refers to parse_comparison
ASTnode *parse_equality(Parser *parser) {
        ASTnode *node = parse_comparison(parser);

        while (match(parser, TOKEN_EEQUAL) || match(parser, TOKEN_NEQUAL)) {
                token op_tok = parser->tokens->tokens[parser->current - 1];
                tokenType op = op_tok.type;
                ASTnode *right = parse_comparison(parser);
                node = make_binary_node(node, op, right);
                ast_set_loc(node, op_tok.line, op_tok.col);
        }
        return node;
}
// parsing && (and operator)
// refers to parse_equality
ASTnode *parse_logical_and(Parser *parser) {
        ASTnode *node = parse_equality(parser);

        while (match(parser, TOKEN_AND)) {
                token op_tok = parser->tokens->tokens[parser->current - 1];
                tokenType op = op_tok.type;
                ASTnode *right = parse_equality(parser);
                node = make_binary_node(node, op, right);
                ast_set_loc(node, op_tok.line, op_tok.col);
        }
        return node;
}
// parsing || (or operator)
// refers to parse_logical_and
ASTnode *parse_logical_or(Parser *parser) {
        ASTnode *node = parse_logical_and(parser);

        while (match(parser, TOKEN_OR)) {
                token op_tok = parser->tokens->tokens[parser->current - 1];
                tokenType op = op_tok.type;
                ASTnode *right = parse_logical_and(parser);
                node = make_binary_node(node, op, right);
                ast_set_loc(node, op_tok.line, op_tok.col);
        }
        return node;
}
// parses ternary operations
// parses and calls parse_logical_or
ASTnode *parse_ternary(Parser *parser) {
        ASTnode *node = parse_logical_or(parser);

        ASTnode *then_expr = NULL;
        ASTnode *else_expr = NULL;
        if (match(parser, TOKEN_QUESTION)) {
                token q_tok = parser->tokens->tokens[parser->current - 1];
                then_expr = parse_ternary(parser);
                consume(parser, TOKEN_COLON, "':'");
                else_expr = parse_ternary(parser);
                node = make_ternary_node(node, then_expr, else_expr);
                ast_set_loc(node, q_tok.line, q_tok.col);
        }
        return node;
}
// parsing assignition
// parses and calls parse_ternary
ASTnode *parse_assignment(Parser *parser) {
        ASTnode *node = parse_ternary(parser);

        if (match(parser, TOKEN_EQUAL)) {
                token trigger = parser->tokens->tokens[parser->current - 1];
                ASTnode *value = parse_assignment(parser);
                if (node->type == NODE_ARRAY_ACCESS) {
                        // detach the borrowed fields so free_ast_node won't free them
                        char *name = node->data.array_access.name;
                        ASTnode *index = node->data.array_access.index;
                        int line = node->line;
                        int col = node->col;
                        node->data.array_access.name = NULL;
                        node->data.array_access.index = NULL;
                        free_ast_node(node);
                        ASTnode *assign = make_array_assign_node(name, index, value);
                        ast_set_loc(assign, line, col);
                        free(name);
                        return assign;
                }
                if (node->type != NODE_IDENTIFIER) {
                        quil_error_at(STAGE_PARSER, ERR_INVALID_ASSIGN_TARGET, trigger.line, trigger.col, node_type_name(node->type));
                }
                int line = node->line;
                int col = node->col;
                char *name = strdup(node->data.identifier.name);
                free_ast_node(node);
                ASTnode *assign = make_assign_node(name, value);
                ast_set_loc(assign, line, col);
                free(name);
                return assign;
        }
        // compound assignments: +=, -=, *=, /=, %=
        if (match(parser, TOKEN_PEQUAL) || match(parser, TOKEN_MEQUAL) ||
            match(parser, TOKEN_SEQUAL) || match(parser, TOKEN_FSEQUAL) ||
            match(parser, TOKEN_PCEQUAL)) {
                token trigger = parser->tokens->tokens[parser->current - 1];
                ASTnode *value = parse_assignment(parser);
                if (node->type != NODE_IDENTIFIER) {
                        quil_error_at(STAGE_PARSER, ERR_INVALID_ASSIGN_TARGET, trigger.line, trigger.col, node_type_name(node->type));
                }
                int line = node->line;
                int col = node->col;
                char *name = strdup(node->data.identifier.name);
                free_ast_node(node);
                // i += v  →  i = (i + v)
                tokenType op = TOKEN_PLUS;
                switch (trigger.type) {
                case TOKEN_MEQUAL: op = TOKEN_MINUS; break;
                case TOKEN_SEQUAL: op = TOKEN_STAR; break;
                case TOKEN_FSEQUAL: op = TOKEN_FSLASH; break;
                case TOKEN_PCEQUAL: op = TOKEN_PERCENT; break;
                default: break;
                }
                ASTnode *sum = make_binary_node(make_identifier_node(name), op, value);
                ASTnode *assign = make_assign_node(name, sum);
                ast_set_loc(assign, line, col);
                free(name);
                return assign;
        }
        return node;
}
// Entry point
// refers to parse_assignment
ASTnode *parse_expression(Parser *parser) {
        return parse_assignment(parser);
}
