CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
JSON_CFLAGS ?= $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS ?= $(shell pkg-config --libs json-c 2>/dev/null)
ifeq ($(strip $(JSON_LIBS)),)
JSON_LIBS := -ljson-c
endif
ifeq ($(strip $(JSON_CFLAGS)),)
JSON_CFLAGS := -I/opt/homebrew/include
endif

CORE_BASE_DIR ?= ../core_base
CORE_IO_DIR ?= ../core_io
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_scene_compile.a
LIB_OBJ = $(OBJ_DIR)/core_scene_compile.o
TEST_BIN = $(OBJ_DIR)/core_scene_compile_test
DIFF_BIN = $(OBJ_DIR)/scene_contract_diff

.PHONY: all clean test scene-contract-diff

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_IO_DIR)/build/libcore_io.a:
	$(MAKE) -C $(CORE_IO_DIR)

$(LIB_OBJ): src/core_scene_compile.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_scene_compile_test.c $(LIB) $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(DIFF_BIN): tools/scene_contract_diff.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(JSON_CFLAGS) $< -o $@ $(JSON_LIBS)

scene-contract-diff: $(DIFF_BIN)
	@echo "built $(DIFF_BIN)"

clean:
	rm -rf $(OBJ_DIR)
