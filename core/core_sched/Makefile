CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

INC = -Iinclude
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_sched.a
LIB_OBJ = $(OBJ_DIR)/core_sched.o
TEST_BIN = $(OBJ_DIR)/core_sched_test
FUZZ_BIN = $(OBJ_DIR)/core_sched_fuzz_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/core_sched.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_sched_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

$(FUZZ_BIN): tests/core_sched_fuzz_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

test: $(TEST_BIN) $(FUZZ_BIN)
	./$(TEST_BIN)
	./$(FUZZ_BIN)

clean:
	rm -rf $(OBJ_DIR)
