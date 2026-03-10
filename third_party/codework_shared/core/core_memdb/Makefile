CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
SQLITE_CFLAGS ?= -std=c11 -O2 -DSQLITE_ENABLE_FTS5
AR ?= ar
ARFLAGS ?= rcs

CORE_BASE_DIR ?= ../core_base

OBJ_DIR = build
LIB = $(OBJ_DIR)/libcore_memdb.a
LIB_OBJ = $(OBJ_DIR)/core_memdb.o $(OBJ_DIR)/sqlite3.o
TEST_BIN = $(OBJ_DIR)/core_memdb_test
CLI_BIN = $(OBJ_DIR)/mem_cli
INC = -Iinclude -I$(CORE_BASE_DIR)/include -Iexternal

.PHONY: all tools test clean

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(OBJ_DIR)/core_memdb.o: src/core_memdb.c include/core_memdb.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/sqlite3.o: external/sqlite3.c external/sqlite3.h | $(OBJ_DIR)
	$(CC) $(SQLITE_CFLAGS) -Iexternal -c $< -o $@

$(LIB): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(TEST_BIN): tests/core_memdb_test.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a -o $@

$(CLI_BIN): tools/mem_cli.c tools/mem_cli_args.c tools/mem_cli_args.h tools/mem_cli_output.c tools/mem_cli_output.h tools/mem_cli_db_helpers.c tools/mem_cli_db_helpers.h tools/mem_cli_cmd_read.c tools/mem_cli_cmd_read.h tools/mem_cli_cmd_write_item.c tools/mem_cli_cmd_write_item.h tools/mem_cli_cmd_write_link.c tools/mem_cli_cmd_write_link.h tools/mem_cli_cmd_event.c tools/mem_cli_cmd_event.h $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) tools/mem_cli.c tools/mem_cli_args.c tools/mem_cli_output.c tools/mem_cli_db_helpers.c tools/mem_cli_cmd_read.c tools/mem_cli_cmd_write_item.c tools/mem_cli_cmd_write_link.c tools/mem_cli_cmd_event.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a -o $@

tools: $(CLI_BIN)

test: $(TEST_BIN) $(CLI_BIN)
	./$(TEST_BIN)
	sh tests/core_memdb_cli_smoke.sh ./$(CLI_BIN) $(OBJ_DIR)/core_memdb_cli_smoke.sqlite

clean:
	rm -rf $(OBJ_DIR)
