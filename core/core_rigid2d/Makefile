CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
LDFLAGS ?= -lm

CORE_COLLISION2D_ROOT ?= ../core_collision2d
INC = -Iinclude -I$(CORE_COLLISION2D_ROOT)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_rigid2d.a
LIB_OBJ = $(OBJ_DIR)/core_rigid2d.o
CORE_COLLISION2D_OBJ = $(OBJ_DIR)/core_collision2d.o
TEST_BIN = $(OBJ_DIR)/core_rigid2d_test
PARITY_TEST_BIN = $(OBJ_DIR)/core_rigid2d_parity_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/core_rigid2d.c include/core_rigid2d.h $(CORE_COLLISION2D_ROOT)/include/core_collision2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(CORE_COLLISION2D_OBJ): $(CORE_COLLISION2D_ROOT)/src/core_collision2d.c $(CORE_COLLISION2D_ROOT)/include/core_collision2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(TEST_BIN): tests/core_rigid2d_test.c $(LIB) $(CORE_COLLISION2D_OBJ) include/core_rigid2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_COLLISION2D_OBJ) -o $@ $(LDFLAGS)

$(PARITY_TEST_BIN): tests/core_rigid2d_parity_test.c $(LIB) $(CORE_COLLISION2D_OBJ) include/core_rigid2d.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_COLLISION2D_OBJ) -o $@ $(LDFLAGS)

test: $(TEST_BIN) $(PARITY_TEST_BIN)
	./$(TEST_BIN)
	./$(PARITY_TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
