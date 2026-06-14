CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
CORE_IO_DIR ?= ../core_io
CORE_PACK_DIR ?= ../core_pack
CORE_SIM_DIR ?= ../core_sim
CORE_TRACE_DIR ?= ../core_trace

INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_SIM_DIR)/include -I$(CORE_TRACE_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_sim_trace.a
LIB_OBJ = $(OBJ_DIR)/core_sim_trace.o
TEST_BIN = $(OBJ_DIR)/core_sim_trace_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_IO_DIR)/build/libcore_io.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_IO_DIR)

$(CORE_PACK_DIR)/build/libcore_pack.a: $(CORE_IO_DIR)/build/libcore_io.a
	$(MAKE) -C $(CORE_PACK_DIR)

$(CORE_SIM_DIR)/build/libcore_sim.a:
	$(MAKE) -C $(CORE_SIM_DIR)

$(CORE_TRACE_DIR)/build/libcore_trace.a: $(CORE_PACK_DIR)/build/libcore_pack.a
	$(MAKE) -C $(CORE_TRACE_DIR)

$(LIB_OBJ): src/core_sim_trace.c include/core_sim_trace.h $(CORE_SIM_DIR)/include/core_sim.h $(CORE_TRACE_DIR)/include/core_trace.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_sim_trace_test.c $(LIB) $(CORE_SIM_DIR)/build/libcore_sim.a $(CORE_TRACE_DIR)/build/libcore_trace.a $(CORE_PACK_DIR)/build/libcore_pack.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_SIM_DIR)/build/libcore_sim.a $(CORE_TRACE_DIR)/build/libcore_trace.a $(CORE_PACK_DIR)/build/libcore_pack.a $(CORE_IO_DIR)/build/libcore_io.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
