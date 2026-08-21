CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

INC = -Iinclude
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_headless_job.a
LIB_OBJ = \
	$(OBJ_DIR)/core_headless_job.o \
	$(OBJ_DIR)/core_headless_job_platform.o
TEST_BIN = $(OBJ_DIR)/core_headless_job_test
PLATFORM_TEST_BIN = $(OBJ_DIR)/core_headless_job_platform_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/core_headless_job.o: src/core_headless_job.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_headless_job_platform.o: src/core_headless_job_platform.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_headless_job_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

$(PLATFORM_TEST_BIN): tests/core_headless_job_platform_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

test: $(TEST_BIN) $(PLATFORM_TEST_BIN)
	./$(TEST_BIN)
	./$(PLATFORM_TEST_BIN)
	python3 tests/test_platform_fixtures.py

clean:
	rm -rf $(OBJ_DIR)
