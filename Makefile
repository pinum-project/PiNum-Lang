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
#  github issue at <https://github.com/pinum-project/PiNum-Lang>   #
#  or contact <surjointelligence.team@gmail.com>                   #
#******************************************************************#

# Variables
CC ?= $CC
RCFLAGS += -Wall -Wextra -O3    # cflags for release make
DCFLAGS += -Wall -Wextra -g -O3 # cflags for default make
SRC = src/main.c src/cli.c src/lexer.c src/lexer_filter.c src/parser.c src/methods.c \
      src/ast.c src/codegen_c.c src/helper.c src/error.c src/_hashmap.c src/sema.c
VERSION = $(shell cat VERSION)

# WASI build (playground pinum.wasm). Point WASI_SDK at your wasi-sdk install, e.g.:
#   make wasm WASI_SDK=/home/user/wasi-sdk-25
WASI_SDK ?= /opt/wasi-sdk
WASI_CC ?= $(WASI_SDK)/bin/clang
WASM_TARGET = pinum.wasm
WASMFLAGS += --target=wasm32-wasi -O2 -I include

TARGET = bin/pinum
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

# Compile it to PiNum-Lang/bin/ directory
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
		git clone --depth 1 https://github.com/pinum-project/pinum-vscode.git extras/vscode

# Pinum compiler backend (QBE fork) — pull the latest version, then build the qbe binary
pncb:
	@git submodule update --init --remote pncb
	$(MAKE) -C pncb qbe

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

.PHONY: all test clean nvim install wasm vscode pncb
