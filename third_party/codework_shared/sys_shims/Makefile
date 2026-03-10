CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
LDFLAGS := -lm

BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/sys_shims_smoke_test
PARITY_BIN := $(BUILD_DIR)/sys_shims_type_parity_test
RUNTIME_BIN := $(BUILD_DIR)/sys_shims_runtime_test
MATH_BIN := $(BUILD_DIR)/sys_shims_math_test
EXT_BIN := $(BUILD_DIR)/sys_shims_extended_domains_test
CONF_BIN := $(BUILD_DIR)/sys_shims_conformance_test

.PHONY: all test test-matrix conformance clean

all: test

test: $(TEST_BIN) $(PARITY_BIN) $(RUNTIME_BIN) $(MATH_BIN) $(EXT_BIN)
	./$(TEST_BIN)
	./$(PARITY_BIN)
	./$(RUNTIME_BIN)
	./$(MATH_BIN)
	./$(EXT_BIN)

test-matrix: test
	@if command -v gcc >/dev/null 2>&1; then \
		echo "running gcc matrix build for sys_shims tests"; \
		gcc $(CFLAGS) tests/sys_shims_smoke_test.c -o $(BUILD_DIR)/sys_shims_smoke_test_gcc; \
		gcc $(CFLAGS) tests/sys_shims_type_parity_test.c -o $(BUILD_DIR)/sys_shims_type_parity_test_gcc; \
		gcc $(CFLAGS) tests/sys_shims_runtime_test.c -o $(BUILD_DIR)/sys_shims_runtime_test_gcc; \
		gcc $(CFLAGS) tests/sys_shims_math_test.c -o $(BUILD_DIR)/sys_shims_math_test_gcc $(LDFLAGS); \
		gcc $(CFLAGS) tests/sys_shims_extended_domains_test.c -o $(BUILD_DIR)/sys_shims_extended_domains_test_gcc $(LDFLAGS); \
		./$(BUILD_DIR)/sys_shims_smoke_test_gcc; \
		./$(BUILD_DIR)/sys_shims_type_parity_test_gcc; \
		./$(BUILD_DIR)/sys_shims_runtime_test_gcc; \
		./$(BUILD_DIR)/sys_shims_math_test_gcc; \
		./$(BUILD_DIR)/sys_shims_extended_domains_test_gcc; \
	else \
		echo "gcc not present; skipping gcc matrix build"; \
	fi

conformance: $(CONF_BIN)
	./$(CONF_BIN) $(BUILD_DIR)/sys_shims_conformance_report.json tests/fixtures/regression_cases_v1.csv

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TEST_BIN): tests/sys_shims_smoke_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/sys_shims_smoke_test.c -o $(TEST_BIN) $(LDFLAGS)

$(PARITY_BIN): tests/sys_shims_type_parity_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/sys_shims_type_parity_test.c -o $(PARITY_BIN) $(LDFLAGS)

$(RUNTIME_BIN): tests/sys_shims_runtime_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/sys_shims_runtime_test.c -o $(RUNTIME_BIN) $(LDFLAGS)

$(MATH_BIN): tests/sys_shims_math_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/sys_shims_math_test.c -o $(MATH_BIN) $(LDFLAGS)

$(EXT_BIN): tests/sys_shims_extended_domains_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/sys_shims_extended_domains_test.c -o $(EXT_BIN) $(LDFLAGS)

$(CONF_BIN): tests/sys_shims_conformance_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/sys_shims_conformance_test.c -o $(CONF_BIN) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
