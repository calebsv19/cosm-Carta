CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude -I../core_base/include
LDFLAGS := -lm

BUILD_DIR := build
LIB := $(BUILD_DIR)/libcore_units.a
TEST_BIN := $(BUILD_DIR)/core_units_test

SRC := src/core_units.c
OBJ := $(BUILD_DIR)/core_units.o
TEST_SRC := tests/core_units_test.c

.PHONY: all test clean

all: $(LIB)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJ): $(SRC) include/core_units.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(LIB): $(OBJ)
	ar rcs $(LIB) $(OBJ)

$(TEST_BIN): $(SRC) $(TEST_SRC) include/core_units.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) ../core_base/src/core_base.c -o $(TEST_BIN) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
