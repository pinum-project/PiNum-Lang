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

#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

/*
 * @enum cli_action
 * @brief What the user asked for, once the flags have been parsed.
 */
typedef enum {
        CLI_ACTION_RUN,     // compile a .pn file
        CLI_ACTION_HELP,    // print the usage text
        CLI_ACTION_VERSION, // print the version
        CLI_ACTION_REPAIR,  // reinstall the .pinum-lang directory
        CLI_ACTION_UPDATE   // run the self-updater
} cli_action;

/*
 * @enum cli_out_mode
 * @brief What -o / -oc told us to produce.
 */
typedef enum {
        CLI_OUT_AOUT,   // default: a.out binary (temp .c deleted)
        CLI_OUT_C,      // -o file.c: C source only
        CLI_OUT_BINARY, // -o name: binary only (temp .c deleted)
        CLI_OUT_BOTH    // -oc name: binary + .c kept
} cli_out_mode;

/*
 * @struct cli_options
 * @brief The result of parsing the command line.
 */
typedef struct {
        cli_action action;    // what to do
        const char *filename; // .pn file to compile (CLI_ACTION_RUN only)
        char *out_name;       // value from -o/-oc, NULL if not given
        cli_out_mode out_mode;
        bool debug_lexer;
        bool debug_ast;
        bool use_qbe; // emit QBE IL instead of C (new backend)
} cli_options;

// parses argv[1..], fills *opts; exits on usage errors.
void cli_parse(int argc, char *argv[], cli_options *opts);

// handle the flags '--help'/-h and '--version'/-v
void cli_print_help(void);
void cli_print_version(void);
// handle flags '--repair'/-r; returns EXIT_SUCCESS or EXIT_FAILURE
int cli_repair(void);
// handle '--update'/-u; returns EXIT_SUCCESS or EXIT_FAILURE
int cli_update(void);

// helper: runs a shell command safely. Used by the updater (cli.c) and
// by main.c's compiler step.
int run_cmd(const char *dir, const char *binary, char *const args[]);

#endif // !CLI_H
