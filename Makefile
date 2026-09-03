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
RCFLAGS += -Wall -Wextra -O2    # cflags for release make
DCFLAGS += -Wall -Wextra -g -O2 # cflags for default make
SRC = src/main.c src/cli.c src/lexer.c src/lexer_filter.c src/parser.c src/methods.c \
      src/ast.c src/codegen_c.c src/helper.c src/error.c src/_hashmap.c src/sema.c src/ssagen.c
VERSION = $(shell cat VERSION)
BUILDDIR = build
OBJ = $(SRC:src/%.c=$(BUILDDIR)/src/%.o)
DEP = $(OBJ:.o=.d)
FEATHER_DEP = $(FEATHER_OBJ:.o=.d)

# Feather lib (no main.o - that's feather binary's main)
FEATHER_COMM = util.o parse.o abi.o cfg.o mem.o ssa.o alias.o load.o copy.o \
	       fold.o gvn.o gcm.o simpl.o ifopt.o live.o spill.o rega.o \
               emit.o
# Core / Common sources
FEATHER_CORE_SRC = feather/util.c feather/parse.c feather/abi.c feather/cfg.c feather/mem.c \
                   feather/ssa.c feather/alias.c feather/load.c feather/copy.c feather/fold.c \
                   feather/gvn.c feather/gcm.c feather/simpl.c feather/ifopt.c feather/live.c \
                   feather/spill.c feather/rega.c feather/emit.c

# Architecture-specific sources
AMD64_SRC = feather/amd64/targ.c feather/amd64/sysv.c feather/amd64/isel.c \
            feather/amd64/emit.c feather/amd64/winabi.c

ARM64_SRC = feather/arm64/targ.c feather/arm64/abi.c feather/arm64/isel.c \
            feather/arm64/emit.c

RV64_SRC  = feather/rv64/targ.c feather/rv64/abi.c feather/rv64/isel.c \
            feather/rv64/emit.c

# Include all architectures into FEATHER_SRC
FEATHER_SRC = $(FEATHER_CORE_SRC) $(AMD64_SRC) $(ARM64_SRC) $(RV64_SRC)
FEATHER_OBJ = $(FEATHER_SRC:%.c=$(BUILDDIR)/%.o)

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
$(BUILDDIR)/feather/%.o: feather/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -MMD -MP -c $< -o $@
$(BUILDDIR)/feather/amd64/%.o: feather/amd64/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -MMD -MP -c $< -o $@
$(BUILDDIR)/feather/arm64/%.o: feather/arm64/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -MMD -MP -c $< -o $@
$(BUILDDIR)/feather/rv64/%.o: feather/rv64/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -MMD -MP -c $< -o $@

# Compile it to quil/bin/ directory
$(TARGET): $(OBJ) $(FEATHER_OBJ)
	$(CC) $(DCFLAGS) $(OBJ) $(FEATHER_OBJ) -o $(TARGET)

# compiling without the -g flag so it has smaller binary
release: $(OBJ) $(FEATHER_OBJ)
	@$(MKDIR)
	$(CC) $(RCFLAGS) $(OBJ) $(FEATHER_OBJ) -o $(TARGET)

-include $(DEP)
-include $(FEATHER_DEP)

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
