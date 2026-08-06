CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
CJSON_SRC = ../../shape/external/cjson/cJSON.c
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I../../shape/external
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_scene_view.a
LIB_OBJ = $(OBJ_DIR)/core_scene_view.o
TEST_BIN = $(OBJ_DIR)/core_scene_view_test
TEST_CJSON_OBJ = $(OBJ_DIR)/cJSON.o

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(LIB_OBJ): src/core_scene_view.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(TEST_CJSON_OBJ): $(CJSON_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../../shape/external/cjson -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_scene_view_test.c $(LIB) $(TEST_CJSON_OBJ) $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(TEST_CJSON_OBJ) $(CORE_BASE_DIR)/build/libcore_base.a -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
