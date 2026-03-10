CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -pthread

INC = -Iinclude
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_wake.a
LIB_OBJ = $(OBJ_DIR)/core_wake.o
TEST_BIN = $(OBJ_DIR)/core_wake_test
STRESS_BIN = $(OBJ_DIR)/core_wake_stress_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/core_wake.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_wake_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

$(STRESS_BIN): tests/core_wake_stress_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

test: $(TEST_BIN) $(STRESS_BIN)
	./$(TEST_BIN)
	./$(STRESS_BIN)

clean:
	rm -rf $(OBJ_DIR)
