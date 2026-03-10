CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

INC = -Iinclude
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_time.a

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PLATFORM_OBJ = $(OBJ_DIR)/core_time_mac.o
else
PLATFORM_OBJ = $(OBJ_DIR)/core_time_posix.o
endif

LIB_OBJS = $(OBJ_DIR)/core_time.o $(PLATFORM_OBJ)
TEST_BIN = $(OBJ_DIR)/core_time_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/core_time.o: src/core_time.c src/core_time_internal.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_time_posix.o: src/core_time_posix.c src/core_time_internal.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_time_mac.o: src/core_time_mac.c src/core_time_internal.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

$(TEST_BIN): tests/core_time_test.c $(LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
