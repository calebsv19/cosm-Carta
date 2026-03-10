CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude -I../core_base/include

BUILD_DIR := build
LIB := $(BUILD_DIR)/libcore_theme.a
TEST_BIN := $(BUILD_DIR)/core_theme_test

SRC := src/core_theme.c
OBJ := $(BUILD_DIR)/core_theme.o
TEST_SRC := tests/core_theme_test.c

.PHONY: all test clean

all: $(LIB)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJ): $(SRC) include/core_theme.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(LIB): $(OBJ)
	ar rcs $(LIB) $(OBJ)

$(TEST_BIN): $(SRC) $(TEST_SRC) include/core_theme.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) ../core_base/src/core_base.c -o $(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
