CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

INC = -Iinclude
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_base.a
LIB_OBJ = $(OBJ_DIR)/core_base.o
TEST_BIN = $(OBJ_DIR)/core_base_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/core_base.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_base_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
