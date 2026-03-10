CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

INC = -Iinclude -I../core_time/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_jobs.a
LIB_OBJ = $(OBJ_DIR)/core_jobs.o
TEST_BIN = $(OBJ_DIR)/core_jobs_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/core_jobs.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_jobs_test.c $(LIB) ../core_time/build/libcore_time.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) ../core_time/build/libcore_time.a -o $@

test: ../core_time/build/libcore_time.a $(TEST_BIN)
	./$(TEST_BIN)

../core_time/build/libcore_time.a:
	$(MAKE) -C ../core_time

clean:
	rm -rf $(OBJ_DIR)
