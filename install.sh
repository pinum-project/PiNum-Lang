#!/bin/bash

#*************************************************
# QUIL - Quick Unified Iterative Language
# Language and Compiler toolchain, Frontend
#
# Copyright (c) 2026-present quil-project authors.
# Licensed under the terms of the LICENSE file.
#
# Issues: <https://github.com/quil-project/quil>
#*************************************************

# quil Installation Script
# This script builds and installs quil locally.
# Requirements:
# - C Compiler (GCC 4.8+, Clang 3.5+, or any C99+ compliant compiler)
# - Make
# - POSIX environment (for strdup support)

set -e

REPO_URL="https://github.com/quil-project/quil"
INSTALL_DIR="$HOME/.quil-lang"

# 1. Check for dependencies
echo "Checking for a C compiler and Make..."

# Try to find a usable C compiler
if command -v cc &>/dev/null; then
        CC_BIN="cc"
elif command -v gcc &>/dev/null; then
        CC_BIN="gcc"
elif command -v clang &>/dev/null; then
        CC_BIN="clang"
else
        echo "Error: No C compiler found (cc, gcc, or clang). Please install one."
        exit 1
fi

# Print compiler version for debugging/confirmation
$CC_BIN --version | head -n 1

if ! command -v make &>/dev/null; then
        echo "Error: make not found. Please install 'make'."
        exit 1
fi

# 2. Clone or Update
if [ -d "$INSTALL_DIR" ]; then
        echo "Updating existing installation in $INSTALL_DIR..."
        cd "$INSTALL_DIR"
        # always converge to the remote, even if the local clone has diverged
        git fetch --depth 1 origin
        git reset --hard origin/main
else
        echo "-----------------------------"
        echo "   Installing quil...  "
        echo "-----------------------------"
        echo ""
        echo "Cloning repository to $INSTALL_DIR..."
        git clone --depth 1 --quiet "$REPO_URL" "$INSTALL_DIR"
        cd "$INSTALL_DIR"
fi

# 3. Build
echo "Building Quil..."
make release -s CC=$CC_BIN >/dev/null

# 4. Install
# Checking for termux; sudo wouldn't run on termux
# if it is not termux then it is a unix based OS.
if [ -d "/data/data/com.termux" ] || [ -n "$TERMUX_VERSION" ]; then
        echo "Termux detected! Installing to $PREFIX/bin..."
        make -s install >/dev/null
else
        echo "Installing to /usr/local/bin (requires sudo)..."
        sudo make -s install >/dev/null
fi

# 5. Neovim Syntax (Optional)
# detects for neovim and runs activate_syntax.sh
if [ -d "$HOME/.config/nvim" ] || [ -d "$HOME/.local/share/nvim" ]; then
        echo ""
        echo "Neovim detected!"
        # When piped from curl, stdin is the pipe. We need to read from the terminal (/dev/tty).
        read -p "Do you want to activate Quil syntax highlighting for Neovim? (y/n): " -n 1 -r </dev/tty
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
                echo "Activating Neovim syntax..."
                make -s nvim >/dev/null
        fi
fi

# 6. VS Code Extension (Optional)
# the extension lives in the extras/vscode submodule; a shallow clone of the
# repo does not fetch submodules, so pull it in before looking for the .vsix
if [ ! -f extras/vscode/quil-lang-*.vsix ]; then
        echo "Fetching the VS Code extension..."
        git submodule update --init --depth 1 extras/vscode 2>/dev/null ||
                git clone --depth 1 https://github.com/quil-project/quil-vscode.git extras/vscode 2>/dev/null || true
fi
echo ""
echo "Quil has a VS Code extension for syntax highlighting."
echo "It is recommended if you use VS Code (or Codium)."
read -p "Do you want to install it? (y/n): " -n 1 -r </dev/tty
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
        VSIX_FILE=$(ls extras/vscode/quil-lang-*.vsix 2>/dev/null | head -n 1)
        if command -v code &>/dev/null; then
                echo "Installing Quil VS Code extension..."
                code --install-extension "$VSIX_FILE"
        elif command -v codium &>/dev/null; then
                echo "Installing Quil VS Code extension for Codium..."
                codium --install-extension "$VSIX_FILE"
        else
                echo "VS Code was not found on your system."
                echo "Install it, then run inside vscode: code --install-extension ~/.quil-lang/extras/vscode/quil-lang-*.vsix"
                echo "Or grab the .vsix from the GitHub releases page."
        fi
fi

# deleting the temporary build folder
rm -rf src include example .github payload test bin .gitignore .clang-format Makefile CONTRIBUTING.md install.sh runtime/.gitkeep lib/.gitkeep

echo ""
echo "----------------------------------"
echo "Successfully installed quil!"
echo "Try running: quil"
echo "----------------------------------"
