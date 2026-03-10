CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -pthread

INC = -Iinclude -I../core_queue/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_workers.a
LIB_OBJ = $(OBJ_DIR)/core_workers.o
TEST_BIN = $(OBJ_DIR)/core_workers_test
SOAK_BIN = $(OBJ_DIR)/core_workers_soak_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/core_workers.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_workers_test.c $(LIB) ../core_queue/build/libcore_queue.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) ../core_queue/build/libcore_queue.a -o $@

$(SOAK_BIN): tests/core_workers_soak_test.c $(LIB) ../core_queue/build/libcore_queue.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) ../core_queue/build/libcore_queue.a -o $@

test: ../core_queue/build/libcore_queue.a $(TEST_BIN) $(SOAK_BIN)
	./$(TEST_BIN)
	./$(SOAK_BIN)

../core_queue/build/libcore_queue.a:
	$(MAKE) -C ../core_queue

clean:
	rm -rf $(OBJ_DIR)
