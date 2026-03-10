CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../../core/core_base
CORE_DATA_DIR ?= ../../core/core_data
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_DATA_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libkit_viz.a
LIB_OBJ = $(OBJ_DIR)/kit_viz.o
TEST_BIN = $(OBJ_DIR)/kit_viz_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_DATA_DIR)/build/libcore_data.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_DATA_DIR)

$(LIB_OBJ): src/kit_viz.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/kit_viz_test.c $(LIB) $(CORE_DATA_DIR)/build/libcore_data.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_DATA_DIR)/build/libcore_data.a $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
