/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#include "../include/cli.h"
#include "../include/error.h"
#include "../include/version.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef __wasm__
#include <sys/wait.h>
#include <unistd.h>
#endif

// the parameter 'binary' is the command to execute (searched in $PATH).
// 'dir' is the working directory the command should run in (NULL = inherit current).
#ifndef __wasm__
int run_cmd(const char *dir, const char *binary, char *const args[]) {
        // fork the process
        pid_t pid = fork();

        // child process
        if (pid == 0) {
                // change into the requested working directory, if any
                if (dir != NULL && chdir(dir) < 0) {
                        quil_error(STAGE_UPDATER, ERR_UPDATE_CHDIR, strerror(errno));
                }
                // replace the child process binary with the target command.
                // execvp looks the command up in $PATH instead of assuming an absolute path.
                execvp(binary, args);
                // if execvp reaches this line, it failed
                quil_error(STAGE_UPDATER, ERR_UPDATE_EXEC, strerror(errno));
        }
        // parent process
        else if (pid > 0) {
                int status;
                // pause and wait for the child process to finish
                waitpid(pid, &status, 0);
                // check if child exited with clean code 0
                return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : 1;
        }

        // fork entirely failed
        return -1;
}
#endif

// --- FLAG PARSING ---
void cli_parse(int argc, char *argv[], cli_options *opts) {
        opts->action = CLI_ACTION_RUN;
        opts->filename = NULL;
        opts->out_name = NULL;
        opts->out_mode = CLI_OUT_AOUT;
        opts->debug_lexer = false;
        opts->debug_ast = false;
        opts->use_qbe = false;

        // Exits if user does not provide any file
        if (argc < 2) {
                fprintf(stderr, "Usage: %s [Flags] <file>\n", argv[0]);
                fprintf(stderr, "See '--help' for more info.\n");
                quil_error(STAGE_FILE, ERR_NO_INPUT_FILE, NULL);
        }

        int arg_indx = 1; // argument index. arg index 0 is the program binary name

        // the check below treats an argument that has a '.' in it as a file,
        // otherwise it is a flag/option.
        while (arg_indx < argc && strrchr(argv[arg_indx], '.') == NULL) {
                if (strcmp(argv[arg_indx], "--help") == 0 || strcmp(argv[arg_indx], "-h") == 0) {
                        opts->action = CLI_ACTION_HELP;
                        return;
                } else if (strcmp(argv[arg_indx], "--version") == 0 || strcmp(argv[arg_indx], "-v") == 0) {
                        opts->action = CLI_ACTION_VERSION;
                        return;
                } else if (strcmp(argv[arg_indx], "--repair") == 0 || strcmp(argv[arg_indx], "-r") == 0) {
                        opts->action = CLI_ACTION_REPAIR;
                        return;
                } else if (strcmp(argv[arg_indx], "--update") == 0 || strcmp(argv[arg_indx], "-u") == 0) {
                        opts->action = CLI_ACTION_UPDATE;
                        return;
                } else if (strcmp(argv[arg_indx], "-o") == 0 || strcmp(argv[arg_indx], "--output") == 0) {
                        // check if there is enough args left to hold the output name
                        if (arg_indx + 1 >= argc) {
                                quil_error(STAGE_FILE, ERR_NO_OUTPUT_FILE, NULL);
                        }
                        // consume the name first so the flag loop does not evaluate
                        // a name with a '.' (like file.c) as a flag
                        opts->out_name = argv[++arg_indx];
                        opts->out_mode = (strrchr(opts->out_name, '.') && strcmp(strrchr(opts->out_name, '.'), ".c") == 0) ? CLI_OUT_C : CLI_OUT_BINARY;
                        arg_indx++;
                } else if (strcmp(argv[arg_indx], "-oc") == 0 || strcmp(argv[arg_indx], "--output-c") == 0) {
                        if (arg_indx + 1 >= argc) quil_error(STAGE_FILE, ERR_NO_OUTPUT_FILE, NULL);
                        opts->out_name = argv[++arg_indx];
                        opts->out_mode = CLI_OUT_BOTH;
                        arg_indx++;
                } else if (strcmp(argv[arg_indx], "--debug-all") == 0) {
                        opts->debug_lexer = true;
                        opts->debug_ast = true;
                        arg_indx++;
                } else if (strcmp(argv[arg_indx], "--debug-lexer") == 0) {
                        opts->debug_lexer = true;
                        arg_indx++;
                } else if (strcmp(argv[arg_indx], "--debug-ast") == 0) {
                        opts->debug_ast = true;
                        arg_indx++;
                } else if (strcmp(argv[arg_indx], "--qbe") == 0) {
                        opts->use_qbe = true;
                        arg_indx++;
                }
                // Unrecognized and invalid flag handling
                else {
                        printf("See '--help' for more info.\n");
                        quil_error(STAGE_FILE, ERR_INVALID_FLAG, argv[arg_indx]);
                }
        }

        if (arg_indx >= argc) {
                quil_error(STAGE_FILE, ERR_NO_INPUT_FILE, NULL);
        }
        opts->filename = argv[arg_indx];
}
// -------------------

// --- FLAG HANDLING (functions) ---
// handle the flags '--help' and '-h'
void cli_print_help() {
        printf("quil version %s\n\n", QUIL_VERSION);
        printf("Usage: quil [Flags] <file.quil|file.qil>\n");
        printf("Flags:\n");
        printf("  %-20s\tOutput a C source file (*.c) or a binary (*).\n", "-o, --output");
        printf("  %-20s\tOutput both a C source file and a binary.\n", "-oc, --output-c");
        printf("\n");
        printf("  %-20s\tEnable all debugging functions.\n", "--debug-all");
        printf("  %-20s\tEnable debugging functions for lexer.\n", "--debug-lexer");
        printf("  %-20s\tEnable debugging functions for ast.\n", "--debug-ast");
        printf("\n");
        printf("  %-20s\tCompile via the QBE backend (emits QBE IL).\n", "--qbe");
        printf("\n");
        printf("  %-20s\tDisplay quil version information.\n", "-v, --version");
        printf("  %-20s\tUpdate quil to the latest version.\n", "-u, --update");
        printf("  %-20s\tReinstall to get ~/.quil-lang directory back.\n", "-r, --repair");
        printf("  %-20s\tDisplay this output.\n", "-h, --help");
        printf("\nIf you find any issue, create a github issue at <https://github.com/quil-project/quil>\n");
}

// handle the flags '--version' and '-v'
void cli_print_version() {
        printf("quil version %s\n", QUIL_VERSION);
}

// --- Updating pipeline ---
#ifndef __wasm__
static char g_latest_version[64] = {0};
// returns 1 if an update is available, 0 if up to date; exits on failure.
static int check_update() {
        FILE *online_version = popen("curl -sSL --fail https://raw.githubusercontent.com/quil-project/quil/main/VERSION", "r");
        if (online_version == NULL) {
                quil_error(STAGE_UPDATER, ERR_UPDATE_START, NULL);
        }

        char version[64] = {0};
        if (fgets(version, sizeof(version), online_version) != NULL) {
                // remove trailing newlines if there are
                version[strcspn(version, "\r\n")] = '\0';
        }
        snprintf(g_latest_version, sizeof(g_latest_version), "%s", version);

        int status = pclose(online_version);
        if (status != 0 || version[0] == '\0') {
                quil_error(STAGE_UPDATER, ERR_UPDATE_CHECK, NULL); // fetch failed
        }
        // if version != QUIL_VERSION it returns 1 (update available),
        // if version == QUIL_VERSION it returns 0 (up to date)
        return strcmp(version, QUIL_VERSION) != 0;
}
static char g_update_dir[512] = {0};
// removes the two temp files and the temp directory used by the updater
static void cleanup_temp(void) {
        if (g_update_dir[0] == '\0') {
                return;
        }
        char installer_path[1024];
        char hash_path[1024];
        snprintf(installer_path, sizeof(installer_path), "%s/install.sh", g_update_dir);
        snprintf(hash_path, sizeof(hash_path), "%s/install.sh.sha256", g_update_dir);
        remove(installer_path);
        remove(hash_path);
        rmdir(g_update_dir);
        g_update_dir[0] = '\0';
}
// downloads the installer and its checksum, then verifies the hash; exits on any failure.
static void check_hash() {
        // pick a temp directory that exists (Termux uses $TMPDIR, not /tmp)
        const char *tmpdir = getenv("TMPDIR");
        if (tmpdir == NULL) tmpdir = "/tmp";

        // creating a unique directory
        char dir_template[1024];
        snprintf(dir_template, sizeof(dir_template), "%s/quil_update_XXXXXX", tmpdir);
        char *tmp_dir = mkdtemp(dir_template);
        if (!tmp_dir) {
                quil_error(STAGE_UPDATER, ERR_UPDATE_TMP_DIR, NULL);
        }
        strncpy(g_update_dir, tmp_dir, sizeof(g_update_dir) - 1);

        // absolute file paths in the temp directory
        char installer_path[1024];
        char hash_path[1024];
        snprintf(installer_path, sizeof(installer_path), "%s/install.sh", g_update_dir);
        snprintf(hash_path, sizeof(hash_path), "%s/install.sh.sha256", g_update_dir);

        // script url, pinned to the same release tag as the checksum
        char script_url[1024] = {0};
        snprintf(script_url, sizeof(script_url), "https://raw.githubusercontent.com/quil-project/quil/v%s/install.sh", g_latest_version);

        // downloading install.sh into the installer_path
        printf("Downloading installer script (install.sh)...\n");
        char *curl_installer_args[] = {
            "curl",
            "-sSL",
            "--fail",
            script_url,
            "-o",
            installer_path,
            NULL,
        };
        // curl installation failed
        if (run_cmd(NULL, "curl", curl_installer_args) != 0) {
                cleanup_temp();
                quil_error(STAGE_UPDATER, ERR_UPDATE_DOWNLOAD_SCRIPT, NULL);
        }

        // hash url
        char hash_url[1024] = {0};
        snprintf(hash_url, sizeof(hash_url), "https://github.com/quil-project/quil/releases/download/v%s/install.sh.sha256", g_latest_version);

        // downloading install.sh.sha256 into the hash_path
        printf("Downloading checksum file (install.sh.sha256)...\n");
        char *curl_hash_args[] = {
            "curl",
            "-sSL",
            "--fail",
            hash_url,
            "-o",
            hash_path,
            NULL,
        };
        // curl installation failed
        if (run_cmd(NULL, "curl", curl_hash_args) != 0) {
                cleanup_temp();
                quil_error(STAGE_UPDATER, ERR_UPDATE_DOWNLOAD_CHECKSUM, NULL);
        }

        // verify the checksum in the temp directory so the relative
        // "install.sh" listed in the checksum file resolves correctly.
        // macOS ships `shasum` with different flags than Linux `sha256sum`.
#ifdef __APPLE__
        const char *sha_cmd = "shasum";
        char *sha_args[] = {"shasum", "-a", "256", "--check", "--status", "install.sh.sha256", NULL};
#else
        const char *sha_cmd = "sha256sum";
        char *sha_args[] = {"sha256sum", "--check", "--status", "install.sh.sha256", NULL};
#endif
        if (run_cmd(g_update_dir, sha_cmd, sha_args) != 0) {
                cleanup_temp();
                quil_error(STAGE_UPDATER, ERR_UPDATE_CHECKSUM_VERIFY, NULL);
        }
        // checksum passed; keep files for install step
}
// handle the flags '--update' and '-u'
int cli_update() {
        printf("Checking for updates...\n");
        int check = check_update();
        if (check == 0) {
                // quil up to date
                printf("Up to date!\n");
                return EXIT_SUCCESS;
        }
        // downloads and verifies the installer; exits on any failure
        check_hash();

        printf("Verification successful!\n");
        printf("\nExecuting installer...\n");
        char *bash_args[] = {
            "bash",
            "install.sh",
            NULL,
        };
        int result = run_cmd(g_update_dir, "bash", bash_args);

        cleanup_temp();

        return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// handle flags '--repair'/-r
int cli_repair(void) {
        // fetch the latest version first so check_hash() builds valid download URLs
        check_update();
        // downloads and verifies the installer; exits on any failure
        check_hash();

        printf("Verification successful!\n");
        printf("\nExecuting installer...\n");
        char *bash_args[] = {
            "bash",
            "install.sh",
            NULL,
        };
        int result = run_cmd(g_update_dir, "bash", bash_args);

        cleanup_temp();

        return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
