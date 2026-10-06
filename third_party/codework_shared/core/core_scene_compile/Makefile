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
CORE_SCENE_DIR ?= ../core_scene
CORE_OBJECT_DIR ?= ../core_object
CORE_UNITS_DIR ?= ../core_units
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_scene_compile.a
LIB_OBJ = $(OBJ_DIR)/core_scene_compile.o $(OBJ_DIR)/core_scene_compile_digest.o \
	$(OBJ_DIR)/core_scene_compile_dependencies.o $(OBJ_DIR)/core_scene_compile_bundle.o \
	$(OBJ_DIR)/core_scene_compile_payload.o $(OBJ_DIR)/core_scene_compile_verify.o
TEST_BIN = $(OBJ_DIR)/core_scene_compile_test
DIFF_BIN = $(OBJ_DIR)/scene_contract_diff
COMPILE_TOOL_BIN = $(OBJ_DIR)/scene_compile_tool

.PHONY: all clean test scene-contract-diff scene-compile-tool

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_IO_DIR)/build/libcore_io.a:
	$(MAKE) -C $(CORE_IO_DIR)

$(CORE_SCENE_DIR)/build/libcore_scene.a:
	$(MAKE) -C $(CORE_SCENE_DIR)

$(CORE_OBJECT_DIR)/build/libcore_object.a:
	$(MAKE) -C $(CORE_OBJECT_DIR)

$(CORE_UNITS_DIR)/build/libcore_units.a:
	$(MAKE) -C $(CORE_UNITS_DIR)

$(OBJ_DIR)/core_scene_compile.o: src/core_scene_compile.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_scene_compile_digest.o: src/core_scene_compile_digest.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_scene_compile_dependencies.o: src/core_scene_compile_dependencies.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_scene_compile_bundle.o: src/core_scene_compile_bundle.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_scene_compile_payload.o: src/core_scene_compile_payload.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_scene_compile_verify.o: src/core_scene_compile_verify.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_scene_compile_test.c $(LIB) $(DIFF_BIN) $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a $(CORE_SCENE_DIR)/build/libcore_scene.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(JSON_CFLAGS) $(INC) $< $(LIB) $(CORE_SCENE_DIR)/build/libcore_scene.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a $(JSON_LIBS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(DIFF_BIN): tools/scene_contract_diff.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(JSON_CFLAGS) $< -o $@ $(JSON_LIBS)

scene-contract-diff: $(DIFF_BIN)
	@echo "built $(DIFF_BIN)"

$(COMPILE_TOOL_BIN): tools/scene_compile_tool.c $(LIB) $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a $(CORE_SCENE_DIR)/build/libcore_scene.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_SCENE_DIR)/build/libcore_scene.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

scene-compile-tool: $(COMPILE_TOOL_BIN)
	@echo "built $(COMPILE_TOOL_BIN)"

clean:
	rm -rf $(OBJ_DIR)
