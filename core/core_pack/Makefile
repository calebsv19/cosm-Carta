CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
CORE_IO_DIR ?= ../core_io
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_pack.a
LIB_OBJ = $(OBJ_DIR)/core_pack.o $(OBJ_DIR)/core_pack_vf2d.o
TEST_BIN = $(OBJ_DIR)/core_pack_test
CLI_BIN = $(OBJ_DIR)/pack_cli

.PHONY: all clean test tools fixtures-refresh

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_IO_DIR)/build/libcore_io.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_IO_DIR)

$(OBJ_DIR)/core_pack.o: src/core_pack.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_pack_vf2d.o: src/core_pack_vf2d.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_pack_test.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a $(CORE_IO_DIR)/build/libcore_io.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

$(CLI_BIN): tools/pack_cli.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a $(CORE_IO_DIR)/build/libcore_io.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

tools: $(CLI_BIN)

test: $(TEST_BIN) $(CLI_BIN)
	./$(TEST_BIN)
	./$(CLI_BIN) roundtrip /tmp/core_pack_cli_test.pack
	cd tests/fixtures && shasum -a 256 -c fixtures.sha256
	./$(CLI_BIN) inspect tests/fixtures/physics_v1_sample.pack > /tmp/core_pack_fixture_physics.inspect.txt
	./$(CLI_BIN) inspect tests/fixtures/daw_v1_sample.pack > /tmp/core_pack_fixture_daw.inspect.txt

clean:
	rm -rf $(OBJ_DIR)

fixtures-refresh: $(CLI_BIN)
	./tests/fixtures/regenerate_fixtures.sh
