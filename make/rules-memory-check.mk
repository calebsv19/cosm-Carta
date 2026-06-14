# =========================
#  fisiCs memory-check audit
# =========================

MEMORY_CHECK_FISICS_OVERLAY := physics-units,memory-check
MEMORY_CHECK_REPORT_DIR := build/memory_check
MEMORY_CHECK_STDOUT := $(MEMORY_CHECK_REPORT_DIR)/map_forge.stdout
MEMORY_CHECK_STDERR := $(MEMORY_CHECK_REPORT_DIR)/map_forge.stderr
MEMORY_CHECK_OBJ_DIR := $(TARGET_BUILD_ROOT)/toolchains/fisics/memory_check_obj
MEMORY_CHECK_BIN := $(TARGET_BUILD_ROOT)/toolchains/fisics/bin/mapforge_memory_check_route_service_test
MEMORY_CHECK_SRCS := $(APP_ROUTE_SERVICE_TEST_SRCS)
MEMORY_CHECK_OBJS := $(patsubst %.c,$(MEMORY_CHECK_OBJ_DIR)/%.o,$(MEMORY_CHECK_SRCS))
MEMORY_CHECK_REPORT_POLICY ?= always
FISICS_MEMCHECK_RUNTIME ?= /Users/calebsv/Desktop/CodeWork/fisiCs/build/unsanitized/libfisics_memcheck_runtime.a

$(MEMORY_CHECK_OBJ_DIR)/%.o: %.c $(APP_COMPILER_STAMP)
	@mkdir -p $(dir $@)
	$(APP_CC) $(APP_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(MEMORY_CHECK_BIN): $(MEMORY_CHECK_OBJS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $(MEMORY_CHECK_OBJS) "$(FISICS_MEMCHECK_RUNTIME)" -o $@ -lm

memory-check-build:
	@$(MAKE) BUILD_TOOLCHAIN=fisics ARCH_FLAGS= APP_CC="$(FISICS_CC) --overlay=$(MEMORY_CHECK_FISICS_OVERLAY)" -B "$(MEMORY_CHECK_BIN)"

memory-check-run: memory-check-build
	@mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(MEMORY_CHECK_BIN)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"
	@echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"
	@echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"

memory-check-audit: memory-check-run
	@echo "memory-check summary:"
	@grep -E "\\[fisics:memory-check\\] (summary|leak|double free|unknown pointer free)" "$(MEMORY_CHECK_STDERR)" || true
