CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
CORE_OBJECT_DIR ?= ../core_object
CORE_UNITS_DIR ?= ../core_units
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_scene.a
LIB_OBJ = $(OBJ_DIR)/core_scene.o
TEST_BIN = $(OBJ_DIR)/core_scene_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_OBJECT_DIR)/build/libcore_object.a:
	$(MAKE) -C $(CORE_OBJECT_DIR)

$(CORE_UNITS_DIR)/build/libcore_units.a:
	$(MAKE) -C $(CORE_UNITS_DIR)

$(LIB_OBJ): src/core_scene.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_scene_test.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
