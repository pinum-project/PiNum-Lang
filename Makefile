#*************************************************
# QUIL - Quick Unified Iterative Language
# Language and Compiler toolchain, Frontend
#
# Copyright (c) 2026-present quil-project authors.
# Licensed under the terms of the LICENSE file.
#
# Issues: <https://github.com/quil-project/quil>
#*************************************************

# Variables
CC ?= $CC
RCFLAGS += -Wall -Wextra -O3    # cflags for release make
DCFLAGS += -Wall -Wextra -g -O3 # cflags for default make
SRC = src/main.c src/cli.c src/lexer.c src/lexer_filter.c src/parser.c src/methods.c \
      src/ast.c src/codegen_c.c src/helper.c src/error.c src/_hashmap.c src/sema.c
VERSION = $(shell cat VERSION)

# WASI build (playground quil.wasm). Point WASI_SDK at your wasi-sdk install, e.g.:
#   make wasm WASI_SDK=/home/user/wasi-sdk-25
WASI_SDK ?= /opt/wasi-sdk
WASI_CC ?= $(WASI_SDK)/bin/clang
WASM_TARGET = quil.wasm
WASMFLAGS += --target=wasm32-wasi -O2 -I include

TARGET = bin/quil
MKDIR = mkdir -p bin
RM = rm -f

# Check for Termux
ifneq ($(wildcard /data/data/com.termux/files/usr/bin/*),)
    INSTALL_PATH ?= $(PREFIX)/bin
else
    INSTALL_PATH ?= /usr/local/bin
endif

# The default rule
all: $(TARGET)

# Compile it to quil/bin/ directory
$(TARGET): $(SRC)
	@$(MKDIR)
	$(CC) $(DCFLAGS) $(SRC) -o $(TARGET)

# compiling without the -g flag so it has smaller binary
release: $(SRC)
	@$(MKDIR)
	$(CC) $(RCFLAGS) $(SRC) -o $(TARGET)

# in-browser WASI build (used by the site playground)
wasm: $(SRC)
	$(WASI_CC) $(WASMFLAGS) $(SRC) -o $(WASM_TARGET)

# VS Code extension (init the extras/vscode submodule, falling back to a plain clone)
vscode:
	@git submodule update --init --depth 1 extras/vscode 2>/dev/null || \
		git clone --depth 1 https://github.com/quil-project/quil-vscode.git extras/vscode

# Quil compiler backend (QBE fork) — pull the latest version, then build the feather binary
feather:
	@git submodule update --init --depth 1 feather 2>/dev/null || \
		git clone --depth 1 https://github.com/quil-project/feather.git feather
	@git -C feather fetch --depth 1 origin 2>/dev/null && git -C feather reset --hard origin/main 2>/dev/null || true
	$(MAKE) -C feather

# To install it locally
install: $(TARGET)
	mv $(TARGET) $(INSTALL_PATH)/

# Rule to clean up the binary
clean:
	$(RM) $(TARGET) $(WASM_TARGET)

# Neovim syntax activation
nvim:
	@$(MKDIR)
	chmod +x activate_syntax.sh && ./activate_syntax.sh

.PHONY: all test clean nvim install wasm vscode feather
