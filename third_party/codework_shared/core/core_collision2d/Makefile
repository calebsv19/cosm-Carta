CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
LDFLAGS ?= -lm

INC = -Iinclude
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_collision2d.a
LIB_OBJS = $(OBJ_DIR)/core_collision2d.o $(OBJ_DIR)/core_collision2d_compound.o
TEST_BIN = $(OBJ_DIR)/core_collision2d_test
PARITY_TEST_BIN = $(OBJ_DIR)/core_collision2d_parity_test
HARDENING_TEST_BIN = $(OBJ_DIR)/core_collision2d_hardening_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/core_collision2d.o: src/core_collision2d.c include/core_collision2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_collision2d_compound.o: src/core_collision2d_compound.c include/core_collision2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

$(TEST_BIN): tests/core_collision2d_test.c $(LIB) include/core_collision2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@ $(LDFLAGS)

$(PARITY_TEST_BIN): tests/core_collision2d_parity_test.c $(LIB) include/core_collision2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@ $(LDFLAGS)

$(HARDENING_TEST_BIN): tests/core_collision2d_hardening_test.c $(LIB) include/core_collision2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@ $(LDFLAGS)

test: $(TEST_BIN) $(PARITY_TEST_BIN) $(HARDENING_TEST_BIN)
	./$(TEST_BIN)
	./$(PARITY_TEST_BIN)
	./$(HARDENING_TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
