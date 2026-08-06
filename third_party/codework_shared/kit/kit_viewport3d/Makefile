CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2

INC = -Iinclude
OBJ_DIR = build
LIB = $(OBJ_DIR)/libkit_viewport3d.a
LIB_OBJ = $(OBJ_DIR)/kit_viewport3d.o
TEST_BIN = $(OBJ_DIR)/kit_viewport3d_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/kit_viewport3d.c include/kit_viewport3d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/kit_viewport3d_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -lm -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
