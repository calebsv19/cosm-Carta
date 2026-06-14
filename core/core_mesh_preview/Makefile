CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
CORE_IO_DIR ?= ../core_io
CORE_MESH_ASSET_DIR ?= ../core_mesh_asset
CORE_OBJECT_DIR ?= ../core_object
CORE_UNITS_DIR ?= ../core_units
CJSON_SRC = ../../shape/external/cjson/cJSON.c
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I../../shape/external
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_mesh_preview.a
LIB_OBJ = $(OBJ_DIR)/core_mesh_preview.o
TEST_BIN = $(OBJ_DIR)/core_mesh_preview_test
TEST_CJSON_OBJ = $(OBJ_DIR)/cJSON.o

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_IO_DIR)/build/libcore_io.a:
	$(MAKE) -C $(CORE_IO_DIR)

$(CORE_OBJECT_DIR)/build/libcore_object.a:
	$(MAKE) -C $(CORE_OBJECT_DIR)

$(CORE_UNITS_DIR)/build/libcore_units.a:
	$(MAKE) -C $(CORE_UNITS_DIR)

$(CORE_MESH_ASSET_DIR)/build/libcore_mesh_asset.a:
	$(MAKE) -C $(CORE_MESH_ASSET_DIR)

$(OBJ_DIR)/core_mesh_preview.o: src/core_mesh_preview.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(TEST_CJSON_OBJ): $(CJSON_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../../shape/external/cjson -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_mesh_preview_test.c $(LIB) $(TEST_CJSON_OBJ) $(CORE_MESH_ASSET_DIR)/build/libcore_mesh_asset.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(TEST_CJSON_OBJ) $(CORE_MESH_ASSET_DIR)/build/libcore_mesh_asset.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
