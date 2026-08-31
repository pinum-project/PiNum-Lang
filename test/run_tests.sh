#!/bin/bash

#******************************************************************#
#   _____ _ _   _                       _                          #
#  |  __ (_) \ | |                     | |                         #
#  | |__) ||  \| |_   _ _ __ ___ ______| |     __ _ _ __   __ _    #
#  |  ___/ | . ` | | | | '_ ` _ \______| |    / _` | '_ \ / _` |   #
#  | |   | | |\  | |_| | | | | | |     | |___| (_| | | | | (_| |   #
#  |_|   | |_| \_|\__,_|_| |_| |_|     |______\__,_|_| |_|\__, |   #
#                                                          __/ |   #
#                                                         |___/    #
#                                                                  #
#  Copyright (c) 2026 tanvir-techbro.                              #
#  You may opt to use, copy, modify, merge, publish, distribute    #
#  and/or sell copies of the Software, and permit persons to whom  #
#  the Software is furnished to do so, under the conditions of the #
#  LICENSE.                                                        #
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, #
#  EXPRESS OR IMPLIED.                                             #
#                                                                  #
#  If you find any bug you would be highly encouraged to create a  #
#  github issue at <https://github.com/quil-project/quil>   #
#  or contact <surjointelligence.team@gmail.com>                   #
#******************************************************************#

# This script is an automated test runner for quil.
# It allows running specific categories of tests (Lexer, AST, Parser, Codegen)
# and optionally checking for memory leaks using Valgrind.
#
# NOTE: Make sure to run "make" in the root directory before running this script
# to ensure the "bin/quil" executable is up to date.

# ANSI Color Codes for enhanced terminal output
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
NC='\033[0m' # No Color (Reset)

# --- Prerequisite Check ---
# Ensure the Quil compiler binary exists before attempting to run tests
if [ ! -f "bin/quil" ]; then
        echo -e "${RED}Error: bin/quil not found.${NC}"
        echo -e "${YELLOW}Please run 'make' to build the project first.${NC}"
        exit 1
fi

# --- Core Testing Function ---
# logic: iterates through all .quil files in a directory, executes bin/quil on
# them and asserts the compiler exits cleanly (PASS) or not (FAIL).
# Arguments:
#   $1: Directory containing test files
#   $2: Valgrind flag ('y' to enable)
#   $3: Label for display (e.g., "Lexer")
run_tests() {
        local dir=$1
        local use_valgrind=$2
        local label=$3

        # Validate if the test directory actually exists
        if [ ! -d "$dir" ]; then
                echo -e "${RED}Directory $dir does not exist.${NC}"
                return
        fi

        # Glob all .quil files in the specified directory
        files=$(ls "$dir"/*.quil "$dir"/*.qil 2>/dev/null)
        if [ -z "$files" ]; then
                echo -e "${YELLOW}No tests found in $dir.${NC}"
                return
        fi

        echo -e "${CYAN}--- Running $label Tests ---${NC}"

        # Iterate and execute each test file
        for file in $files; do
                test_name=$(basename "$file")
                echo -e "${GREEN}Testing: $test_name${NC}"

                if [ "$use_valgrind" = "y" ]; then
                        # Run with Valgrind to check for memory leaks/errors.
                        # Keep stderr (the leak report) visible; hide the debug dump on stdout.
                        valgrind --leak-check=full --show-leak-kinds=all ./bin/quil --debug-all "$file" >/dev/null
                else
                        # Normal execution
                        ./bin/quil --debug-all "$file" >/dev/null 2>&1
                fi
                exit_code=$?

                if [ "$exit_code" -eq 0 ]; then
                        echo -e "${GREEN}Result: PASS${NC}"
                else
                        echo -e "${RED}Result: FAIL (compiler exited with $exit_code)${NC}"
                fi
                echo -e "${CYAN}----------------------------------------------------${NC}"
                echo ""
        done
}

# --- Semantic Testing Function ---
# runs each .quil file and checks the exit code: files prefixed with
# "valid_" must compile (exit 0), files prefixed with "invalid_" must be
# rejected by semantic analysis (non-zero exit).
# Arguments:
#   $1: Directory containing semantic test files
#   $2: Valgrind flag ('y' to enable)
run_semantic_tests() {
        local dir=$1
        local use_valgrind=$2

        if [ ! -d "$dir" ]; then
                echo -e "${RED}Directory $dir does not exist.${NC}"
                return
        fi

        files=$(ls "$dir"/*.quil "$dir"/*.qil 2>/dev/null)
        if [ -z "$files" ]; then
                echo -e "${YELLOW}No tests found in $dir.${NC}"
                return
        fi

        echo -e "${CYAN}--- Running Semantic Tests ---${NC}"

        for file in $files; do
                test_name=$(basename "$file")
                expect_fail=0
                case "$test_name" in
                invalid_*) expect_fail=1 ;;
                esac

                echo -e "${GREEN}Testing: $test_name${NC}"
                if [ "$use_valgrind" = "y" ]; then
                        # keep stderr (the leak report) visible for the invalid files
                        echo "42" | valgrind --leak-check=full --show-leak-kinds=all ./bin/quil -oc "$file.tmp.out" "$file" >/dev/null
                else
                        echo "42" | ./bin/quil -oc "$file.tmp.out" "$file" >/dev/null 2>&1
                fi
                exit_code=$?

                if [ "$expect_fail" -eq 1 ] && [ "$exit_code" -ne 0 ]; then
                        echo -e "${GREEN}Result: PASS (correctly rejected, exit $exit_code)${NC}"
                elif [ "$expect_fail" -eq 0 ] && [ "$exit_code" -eq 0 ]; then
                        # valid files are full programs: compile+run to confirm they execute
                        if [ -f "$file.tmp.out" ]; then
                                echo "42" | "$file.tmp.out" >/dev/null 2>&1
                                if [ $? -eq 0 ]; then
                                        echo -e "${GREEN}Result: PASS (compiled and ran)${NC}"
                                else
                                        echo -e "${RED}Result: FAIL (compiled but did not run cleanly)${NC}"
                                fi
                        else
                                echo -e "${RED}Result: FAIL (did not produce an executable)${NC}"
                        fi
                elif [ "$expect_fail" -eq 1 ] && [ "$exit_code" -eq 0 ]; then
                        echo -e "${RED}Result: FAIL (invalid file was accepted)${NC}"
                else
                        echo -e "${RED}Result: FAIL (valid file was rejected, exit $exit_code)${NC}"
                fi

                rm -f "$file.tmp.out" "$file.tmp.out.c"
                echo -e "${CYAN}----------------------------------------------------${NC}"
                echo ""
        done
}

# --- Codegen Testing Function ---
# transpiles each .quil file to payload.c, compiles it to the payload binary,
# then runs the binary to make sure it executes and exits cleanly.
# Arguments:
#   $1: Directory containing codegen test files
#   $2: Valgrind flag ('y' to enable)
run_codegen_tests() {
        local dir=$1
        local use_valgrind=$2

        if [ ! -d "$dir" ]; then
                echo -e "${RED}Directory $dir does not exist.${NC}"
                return
        fi

        files=$(ls "$dir"/*.quil "$dir"/*.qil 2>/dev/null)
        if [ -z "$files" ]; then
                echo -e "${YELLOW}No tests found in $dir.${NC}"
                return
        fi

        echo -e "${CYAN}--- Running Codegen Tests ---${NC}"

        for file in $files; do
                test_name=$(basename "$file")
                test_name=${test_name%.quil}
                test_name=${test_name%.qil}
                echo -e "${GREEN}Testing: $test_name${NC} ($(basename "$file"))"

                # each test gets its own output dir under the gitignored payload/
                test_dir="payload/$test_name"
                mkdir -p "$test_dir"

                if [ "$use_valgrind" = "y" ]; then
                        valgrind --leak-check=full --show-leak-kinds=all ./bin/quil -oc "$test_dir/test result" "$file"
                else
                        ./bin/quil -oc "$test_dir/test result" "$file"
                fi

                if [ $? -eq 0 ]; then
                        # run the binary (feed input in case of read())
                        echo "42" | "$test_dir/test result"
                        if [ $? -eq 0 ]; then
                                echo -e "${GREEN}Result: PASS${NC}"
                        else
                                echo -e "${RED}Result: FAIL (binary did not exit cleanly)${NC}"
                        fi
                else
                        echo -e "${RED}Result: FAIL (transpile failed)${NC}"
                fi
                echo -e "${CYAN}----------------------------------------------------${NC}"
                echo ""
        done
}

# --- Runtime Testing Function ---
# transpiles + compiles each .quil, runs the binary, and asserts:
#   - `panic_*` files exit non-zero with "index out of bounds" on stderr
#   - every other file runs and exits cleanly
# Arguments:
#   $1: Directory containing test files
#   $2: Valgrind flag ('y' to enable)
run_runtime_tests() {
        local dir=$1
        local use_valgrind=$2

        if [ ! -d "$dir" ]; then
                echo -e "${RED}Directory $dir does not exist.${NC}"
                return
        fi

        files=$(ls "$dir"/*.quil "$dir"/*.qil 2>/dev/null)
        if [ -z "$files" ]; then
                echo -e "${YELLOW}No tests found in $dir.${NC}"
                return
        fi

        echo -e "${CYAN}--- Running Runtime Tests ---${NC}"

        for file in $files; do
                test_name=$(basename "$file")
                expect_fail=0
                case "$test_name" in
                panic_*) expect_fail=1 ;;
                esac

                echo -e "${GREEN}Testing: $test_name${NC}"
                test_dir="payload/$test_name"
                mkdir -p "$test_dir"

                if [ "$use_valgrind" = "y" ]; then
                        valgrind --leak-check=full --show-leak-kinds=all ./bin/quil -oc "$test_dir/test result" "$file" >/dev/null
                else
                        ./bin/quil -oc "$test_dir/test result" "$file" >/dev/null 2>&1
                fi
                if [ $? -ne 0 ]; then
                        echo -e "${RED}Result: FAIL (transpile failed)${NC}"
                        echo -e "${CYAN}----------------------------------------------------${NC}"
                        echo ""
                        continue
                fi

                if [ "$use_valgrind" = "y" ]; then
                        output=$(valgrind --leak-check=full --show-leak-kinds=all "$test_dir/test result" 2>&1)
                else
                        output=$("$test_dir/test result" 2>&1)
                fi
                exit_code=$?

                if [ "$expect_fail" -eq 1 ] && [ "$exit_code" -ne 0 ] && echo "$output" | grep -q "index out of bounds"; then
                        echo -e "${GREEN}Result: PASS (correctly panicked, exit $exit_code)${NC}"
                elif [ "$expect_fail" -eq 0 ] && [ "$exit_code" -eq 0 ]; then
                        echo -e "${GREEN}Result: PASS (ran cleanly)${NC}"
                elif [ "$expect_fail" -eq 1 ]; then
                        echo -e "${RED}Result: FAIL (expected panic, got exit $exit_code)${NC}"
                else
                        echo -e "${RED}Result: FAIL (binary did not exit cleanly)${NC}"
                fi

                echo -e "${CYAN}----------------------------------------------------${NC}"
                echo ""
        done
}

# --- Main UI and Interaction ---

# Clear screen for a clean user interface
clear

# Display ASCII Art Header
echo -e "${CYAN}"
cat <<'EOF'
 _____ _ _   _                       _
|  __ (_) \ | |                     | |
| |__) ||  \| |_   _ _ __ ___ ______| |     __ _ _ __   __ _
|  ___/ | . ` | | | | '_ ` _ \______| |    / _` | '_ \ / _` |
| |   | | |\  | |_| | | | | | |     | |___| (_| | | | | (_| |
|_|   |_|_| \_|\__,_|_| |_| |_|     |______\__,_|_| |_|\__, |
                                                        __/ |
                                                       |___/
EOF
echo -e "${NC}"
echo -e "${YELLOW}quil Test Runner${NC}"
echo "======================"
echo ""

# Menu selection
echo "Select which tests you want to run:"
echo "1) All tests"
echo "2) Lexer tests"
echo "3) AST tests"
echo "4) Parser tests"
echo "5) Codegen tests"
echo "6) Semantic tests"
echo "7) Runtime tests"
echo "8) Exit"
echo ""

read -p "Enter your choice [1-8]: " choice

# Handle Exit choice
if [ "$choice" -eq 8 ]; then
        echo "Exiting."
        exit 0
fi

# Input validation for menu choice
if [[ ! "$choice" =~ ^[1-7]$ ]]; then
        echo -e "${RED}Invalid choice. Exiting.${NC}"
        exit 1
fi

echo ""
# Prompt for Valgrind usage
read -p "Do you want to run tests with Valgrind? (recommended) [y/n]: " valgrind_choice
# Convert input to lowercase for consistency
valgrind_choice=$(echo "$valgrind_choice" | tr '[:upper:]' '[:lower:]')

# --- Dependency Check for Valgrind ---
if [ "$valgrind_choice" = "y" ]; then
        if ! command -v valgrind &>/dev/null; then
                echo -e "${RED}Error: valgrind is not installed. Please install it or run without it.${NC}"
                exit 1
        fi
fi

echo ""

# --- Execute selected tests based on user input ---
case $choice in
1)
        run_tests "test/lexer-tests" "$valgrind_choice" "Lexer"
        run_tests "test/ast-tests" "$valgrind_choice" "AST"
        run_tests "test/parser-tests" "$valgrind_choice" "Parser"
        run_codegen_tests "test/codegen-tests" "$valgrind_choice"
        run_semantic_tests "test/semantic" "$valgrind_choice"
        run_runtime_tests "test/runtime-tests" "$valgrind_choice"
        ;;
2)
        run_tests "test/lexer-tests" "$valgrind_choice" "Lexer"
        ;;
3)
        run_tests "test/ast-tests" "$valgrind_choice" "AST"
        ;;
4)
        run_tests "test/parser-tests" "$valgrind_choice" "Parser"
        ;;
5)
        run_codegen_tests "test/codegen-tests" "$valgrind_choice"
        ;;
6)
        run_semantic_tests "test/semantic" "$valgrind_choice"
        ;;
7)
        run_runtime_tests "test/runtime-tests" "$valgrind_choice"
        ;;
esac

echo -e "${GREEN}Test execution finished.${NC}"
