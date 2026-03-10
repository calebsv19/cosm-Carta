CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
CORE_IO_DIR ?= ../core_io
CORE_PACK_DIR ?= ../core_pack
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_PACK_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_trace.a
LIB_OBJ = $(OBJ_DIR)/core_trace.o
TEST_BIN = $(OBJ_DIR)/core_trace_test
FIXTURE_BIN = $(OBJ_DIR)/core_trace_fixture_gen
FIXTURE_PACK = tests/fixtures/trace_v1_sample.pack

.PHONY: all clean test fixtures phase7-check

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_IO_DIR)/build/libcore_io.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_IO_DIR)

$(CORE_PACK_DIR)/build/libcore_pack.a: $(CORE_IO_DIR)/build/libcore_io.a
	$(MAKE) -C $(CORE_PACK_DIR)

$(LIB_OBJ): src/core_trace.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_trace_test.c $(LIB) $(CORE_PACK_DIR)/build/libcore_pack.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_PACK_DIR)/build/libcore_pack.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(FIXTURE_BIN): tests/trace_fixture_gen.c $(LIB) $(CORE_PACK_DIR)/build/libcore_pack.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_PACK_DIR)/build/libcore_pack.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

$(FIXTURE_PACK): $(FIXTURE_BIN)
	@mkdir -p tests/fixtures
	./$(FIXTURE_BIN) $(FIXTURE_PACK)

fixtures: $(FIXTURE_PACK)

phase7-check: test fixtures
	$(MAKE) -C $(CORE_PACK_DIR) tools
	$(CORE_PACK_DIR)/build/pack_cli inspect $(FIXTURE_PACK) > /tmp/core_trace_fixture.inspect.txt

clean:
	rm -rf $(OBJ_DIR)
