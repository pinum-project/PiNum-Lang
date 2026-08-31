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

# Define paths
NVIM_CONFIG="$HOME/.config/nvim"
REPO_DIR=$(pwd)

echo "Setting up Quil syntax highlighting for Neovim..."

# Create directories if they don't exist
mkdir -p "$NVIM_CONFIG/ftdetect"
mkdir -p "$NVIM_CONFIG/syntax"

# Symlink instead of Copy
ln -sf "$REPO_DIR/extras/nvim/ftdetect/quil.lua" "$NVIM_CONFIG/ftdetect/quil.lua"
ln -sf "$REPO_DIR/extras/nvim/syntax/quil.vim" "$NVIM_CONFIG/syntax/quil.vim"

echo "Done."
