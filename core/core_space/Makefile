CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
INC = -Iinclude -I$(CORE_BASE_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_space.a
LIB_OBJ = $(OBJ_DIR)/core_space.o
TEST_BIN = $(OBJ_DIR)/core_space_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(LIB_OBJ): src/core_space.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_space_test.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
