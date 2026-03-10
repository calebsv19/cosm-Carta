CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -pthread

INC = -Iinclude -I../core_time/include -I../core_sched/include -I../core_jobs/include -I../core_wake/include -I../core_queue/include -I../core_workers/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_kernel.a
LIB_OBJ = $(OBJ_DIR)/core_kernel.o
TEST_BIN = $(OBJ_DIR)/core_kernel_test
HARNESS_BIN = $(OBJ_DIR)/core_kernel_harness_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_OBJ): src/core_kernel.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_kernel_test.c $(LIB) \
	../core_time/build/libcore_time.a \
	../core_sched/build/libcore_sched.a \
	../core_jobs/build/libcore_jobs.a \
	../core_wake/build/libcore_wake.a \
	../core_queue/build/libcore_queue.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) \
		../core_time/build/libcore_time.a \
		../core_sched/build/libcore_sched.a \
		../core_jobs/build/libcore_jobs.a \
		../core_wake/build/libcore_wake.a \
		../core_queue/build/libcore_queue.a -o $@

$(HARNESS_BIN): tests/core_kernel_harness_test.c $(LIB) \
	../core_time/build/libcore_time.a \
	../core_sched/build/libcore_sched.a \
	../core_jobs/build/libcore_jobs.a \
	../core_wake/build/libcore_wake.a \
	../core_queue/build/libcore_queue.a \
	../core_workers/build/libcore_workers.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) \
		../core_time/build/libcore_time.a \
		../core_sched/build/libcore_sched.a \
		../core_jobs/build/libcore_jobs.a \
		../core_wake/build/libcore_wake.a \
		../core_queue/build/libcore_queue.a \
		../core_workers/build/libcore_workers.a -o $@

test: \
	../core_time/build/libcore_time.a \
	../core_sched/build/libcore_sched.a \
	../core_jobs/build/libcore_jobs.a \
	../core_wake/build/libcore_wake.a \
	../core_queue/build/libcore_queue.a \
	../core_workers/build/libcore_workers.a \
	$(TEST_BIN) \
	$(HARNESS_BIN)
	./$(TEST_BIN)
	./$(HARNESS_BIN)

../core_time/build/libcore_time.a:
	$(MAKE) -C ../core_time

../core_sched/build/libcore_sched.a:
	$(MAKE) -C ../core_sched

../core_jobs/build/libcore_jobs.a:
	$(MAKE) -C ../core_jobs

../core_wake/build/libcore_wake.a:
	$(MAKE) -C ../core_wake

../core_queue/build/libcore_queue.a:
	$(MAKE) -C ../core_queue

../core_workers/build/libcore_workers.a:
	$(MAKE) -C ../core_workers

clean:
	rm -rf $(OBJ_DIR)
