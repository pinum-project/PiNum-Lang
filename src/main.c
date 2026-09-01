/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

/*
 * src/main.c
 * Main entry, Contains the pipeline
 */

#include "../include/ast.h"
#include "../include/cli.h"
#include "../include/error.h"
#include "../include/lexer.h"
#include "../include/mode.h"
#include "../include/parser.h"
#include "../include/sema.h"
#include "../include/ssagen.h"
// #include "../include/codegen_c.h" // kept for reference, excluded from pipeline (single backend: ssagen+feather)
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * PIPLELINE: check if the files provided are right -> tokenize line by line ->
 *            put tokens in a single line in a token_list -> pass the token_list to parser ->
 *            turn the list into a ASTnode (syntax tree) -> pass the ast to parser to be parsed ->
 *            check the codegen mode (engine or normal) -> pass the parsed nodes (normal codegen or engine codegen) to be compiled into C ->
 *            generate payload.c -> compile payload.c to payload (linux)
 *
 *            file.quil (input) -> src/main.c -> src/lexer.c src/lexer_filter.c src/helper.c -> src/main.c -> src/parser.c src/helper.c ->
 *            src/ast.c -> src/parser.c -> src/main.c -> src/codegen.c = payload/payload.bin (output)
 *
 *            The CLI (flag parsing, --help/--version/--update) lives in src/cli.c.
 */

// - Compiling -
// returns an absolute path to an available C compiler, or NULL
static const char *find_compiler(void);
static void compile_to(const char *compiler, const char *c_path, const char *bin_path);

// --- MAIN ---
int main(int argc, char *argv[]) {
        cli_options opts;
        cli_parse(argc, argv, &opts);

        // --- NON-RUN ACTIONS (handled entirely by the CLI) ---
        switch (opts.action) {
        case CLI_ACTION_HELP:
                cli_print_help();
                return EXIT_SUCCESS;
        case CLI_ACTION_VERSION:
                cli_print_version();
                return EXIT_SUCCESS;
        case CLI_ACTION_UPDATE:
                // the update flag handles its own output and messaging
                return cli_update();
        case CLI_ACTION_REPAIR:
                // reinstalls the missing .quil-lang directory
                return cli_repair();
        case CLI_ACTION_RUN:
                break; // fall through to the pipeline
        }

        // --- FILE HANDLING ---
        const char *filename = opts.filename;
        const char *extention = strrchr(filename, '.');
        FILE *buffer;

        // Checking if the file extention is valid or not.
        if (extention == NULL) {
                quil_error(STAGE_FILE, ERR_INVALID_FILE_TYPE, NULL);
        } else if (strcmp(extention, ".quil") == 0 || strcmp(extention, ".qil") == 0) {
                // checking if the file can be opened or not
                if ((buffer = fopen(filename, "r")) == NULL) {
                        quil_error(STAGE_FILE, ERR_CANNOT_OPEN_FILE, filename);
                }
                // tell the error reporter which file compile errors refer to
                error_set_source_file(filename);
                // If the file open is succesful it will continue with rest of the program.
        } else {
                quil_error(STAGE_FILE, ERR_INVALID_FILE_TYPE, NULL);
        }
        // ---------------------

        // --- MAIN ---
        // Running the loop till we hit EOF (End Of File).
        token_list list;
        token_list_init(&list);
        token tokens = lexer_tokenizer(buffer);
        while (tokens.type != TOKEN_EOF) {
                // checking the tokens for specific types before adding it to the list
                if (tokens.type == TOKEN_HASHTAG) {
                        tokens = token_ignore_comment(tokens, buffer);
                        if (tokens.type == TOKEN_NLINE) {
                                token_list_add(&list, tokens);
                                if (opts.debug_lexer) {
                                        // NOTE: this function call is for debugging purposes.
                                        lexer_print_token(tokens);
                                }
                                // Get next token for the next line
                                tokens = lexer_tokenizer(buffer);
                        }
                        continue;
                }

                token_list_add(&list, tokens);
                if (opts.debug_lexer) {
                        // NOTE: this function call is for debugging purposes.
                        lexer_print_token(tokens);
                }
                // Update tokens for the next iteration
                tokens = lexer_tokenizer(buffer);
        }
        token_list_add(&list, tokens);

        // checking program mode if ENGINE_MODE is not enabled
        if (!ENGINE_MODE) check_program_mode(&list);

        ASTnode *ast = parse(&list);
        if (opts.debug_ast) {
                // NOTE: this function call is for debugging purposes.
                print_ast(ast, 0);
        }

        // --- SEMANTIC ANALYSIS ---
        // catches undeclared/redeclared variables before codegen
        semantic_analyze(ast);

        // --- CODE GENERATION (single backend: ssagen -> feather -> asm) ---
        // codegen_c kept on disk for reference but excluded from pipeline
        char asm_buf[1024];
        char *asm_path = "a.out.s";
        if (opts.out_mode == CLI_OUT_C) {
                asm_path = opts.out_name;
        } else if (opts.out_mode == CLI_OUT_BINARY) {
                snprintf(asm_buf, sizeof(asm_buf), "%s.tmp.s", opts.out_name);
                asm_path = asm_buf;
        } else if (opts.out_mode == CLI_OUT_BOTH) {
                snprintf(asm_buf, sizeof(asm_buf), "%s.s", opts.out_name);
                asm_path = asm_buf;
        } else if (opts.out_mode == CLI_OUT_AOUT) {
                asm_path = "a.out.tmp.s";
        }
        Fn *fn = ssagen_build(ast);
        FILE *asm_out = fopen(asm_path, "w");
        if (asm_out == NULL) {
                quil_error(STAGE_CODEGEN, ERR_CANNOT_OPEN_FILE, asm_path);
        }
        ssagen_emit_asm(fn, asm_out);
        fclose(asm_out);

        free_ast_node(ast);
        // freeing the list and its tokens' values
        token_list_free(&list);

        const char *compiler = find_compiler();
        // compile asm -> binary (feather emits assembly)
        if (opts.out_mode == CLI_OUT_AOUT) {
                compile_to(compiler, asm_path, opts.out_name);
                remove(asm_path);
        } else if (opts.out_mode == CLI_OUT_BINARY) {
                compile_to(compiler, asm_path, opts.out_name);
                remove(asm_path); // delete temp .s
        } else if (opts.out_mode == CLI_OUT_BOTH) {
                compile_to(compiler, asm_path, opts.out_name);
        } else if (opts.out_mode == CLI_OUT_C) {
                // -o file.s : keep asm, no binary
        }

        // NOTE: 2 if statement below are and for debugging purposes.
        /*
        if (ENGINE_MODE) {
                printf("Enabled.\n");
        } else {
                printf("Disabled.\n");
        }
        */
        // ------------

        // --- CLEAN UP AND FINALIZATION ---
        // closing the file buffer
        fclose(buffer);
        return EXIT_SUCCESS;
        // ---------------------------------
}

// --- Compiling ---
static const char *find_compiler(void) {
        // check for overriden custom CC veriable
        const char *env = getenv("CC");
        if (env && env[0] != '\0') {
                return env;
        }
        static char path[1027];
        const char *cands[] = {"cc", "gcc", "clang", "tcc", NULL};
        const char *paths = getenv("PATH"); // list of directories to search
        if (!paths) {
                return NULL;
        }
        char dir[1024];
        const char *p = paths; // keeping track of the current path
        while (*p) {
                const char *end = strchr(p, ':');
                size_t len = end ? (size_t)(end - p) : strlen(p);
                snprintf(dir, sizeof(dir), "%.*s", (int)len, p);
                for (int i = 0; cands[i]; i++) {
                        snprintf(path, sizeof(path), "%s/%s", dir, cands[i]);
                        if (access(path, X_OK) == 0) {
                                return path;
                        }
                }
                if (!end) {
                        break;
                }
                p = end + 1;
        }
        return NULL;
}
static void compile_to(const char *compiler, const char *c_path, const char *bin_path) {
        if (!compiler) {
                quil_error(STAGE_CODEGEN, ERR_NO_COMPILER, NULL);
        }

        char *args[6];
        args[0] = (char *)compiler;
        args[1] = "-O3";
        if (bin_path) {
                args[2] = "-o";
                args[3] = (char *)bin_path;
                args[4] = (char *)c_path;
                args[5] = NULL;
        } else {
                args[2] = (char *)c_path;
                args[3] = NULL;
        }
        if (run_cmd(NULL, compiler, args) != 0) {
                quil_error(STAGE_CODEGEN, ERR_COMPILE_FAILED, c_path);
        }
}
