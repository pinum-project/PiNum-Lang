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
      src/ast.c src/codegen_c.c src/helper.c src/error.c src/_hashmap.c src/sema.c src/ssagen.c
VERSION = $(shell cat VERSION)
BUILDDIR = build
OBJ = $(SRC:src/%.c=$(BUILDDIR)/src/%.o)
DEP = $(OBJ:.o=.d)

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

# Per-file objects (incremental, parallel)
$(BUILDDIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -MMD -MP -c $< -o $@

# Compile it to quil/bin/ directory
$(TARGET): $(OBJ)
	@$(MKDIR)
	$(CC) $(DCFLAGS) $(OBJ) -o $(TARGET)

# compiling without the -g flag so it has smaller binary
release: $(OBJ)
	@$(MKDIR)
	$(CC) $(RCFLAGS) $(OBJ) -o $(TARGET)

-include $(DEP)

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
	$(RM) -r $(BUILDDIR) $(TARGET)

# Neovim syntax activation
nvim:
	@$(MKDIR)
	chmod +x activate_syntax.sh && ./activate_syntax.sh

.PHONY: all test clean nvim install vscode feather
