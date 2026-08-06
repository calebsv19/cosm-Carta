CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
GLSLC ?= glslc
SHADER_MODE ?= compile
PREBUILT_SHADER_DIR ?= prebuilt/shaders
PREBUILT_EXPORT_DIR ?= prebuilt/shaders
MODULE_VERSION := $(strip $(shell cat VERSION))

ifeq ($(MODULE_VERSION),)
  $(error VERSION must contain a non-empty vk_runtime module version)
endif

CSTD ?= -std=c11
CWARN ?= -Wall -Wextra -Werror -Wpedantic
COPT ?= -O2

VULKAN_CFLAGS := $(shell $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(shell $(PKG_CONFIG) --libs vulkan 2>/dev/null)
ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
  VULKAN_CFLAGS := -I/opt/homebrew/include
  VULKAN_LIBS := -L/opt/homebrew/lib -lvulkan
endif

CPPFLAGS := -Iinclude $(VULKAN_CFLAGS) \
	-DVK_RUNTIME_BUILD_VERSION=\"$(MODULE_VERSION)\"
CFLAGS := $(CSTD) $(CWARN) $(COPT)
LDFLAGS := $(VULKAN_LIBS)

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin

LIB := $(LIB_DIR)/libvkruntime.a
SOURCES := src/vk_runtime.c src/vk_runtime_instance.c \
	src/vk_runtime_json.c src/vk_runtime_compute.c \
	src/vk_runtime_resident.c src/vk_runtime_program.c \
	src/vk_runtime_timing.c
OBJECTS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
PROBE := $(BIN_DIR)/vk_runtime_probe
CONTRACT_TEST := $(BIN_DIR)/vk_runtime_contract_test
SANITIZE_TEST := $(BIN_DIR)/vk_runtime_contract_test_sanitize
LIVE_REPORT := $(BUILD_DIR)/capability_report.json
LIVE_REPEAT_REPORT := $(BUILD_DIR)/capability_report_repeat.json
VALIDATION_REPORT := $(BUILD_DIR)/capability_report_validation.json
COMPILED_SHADER_DIR := $(BUILD_DIR)/shaders
SHADER_FLAGS := --target-env=vulkan1.1 -O
COMPILED_U32_SPV := $(COMPILED_SHADER_DIR)/u32_transform.spv
COMPILED_F32_SPV := $(COMPILED_SHADER_DIR)/f32_transform.spv
COMPILED_U32_MANIFEST := $(COMPILED_SHADER_DIR)/u32_transform.manifest.json
COMPILED_F32_MANIFEST := $(COMPILED_SHADER_DIR)/f32_transform.manifest.json
ifeq ($(SHADER_MODE),compile)
  U32_SPV := $(COMPILED_U32_SPV)
  F32_SPV := $(COMPILED_F32_SPV)
  U32_MANIFEST := $(COMPILED_U32_MANIFEST)
  F32_MANIFEST := $(COMPILED_F32_MANIFEST)
  SHADER_BUNDLE_PREREQUISITE :=
  SHADER_BUNDLE_VALIDATION :=
else ifeq ($(SHADER_MODE),prebuilt)
  U32_SPV := $(PREBUILT_SHADER_DIR)/u32_transform.spv
  F32_SPV := $(PREBUILT_SHADER_DIR)/f32_transform.spv
  U32_MANIFEST := $(PREBUILT_SHADER_DIR)/u32_transform.manifest.json
  F32_MANIFEST := $(PREBUILT_SHADER_DIR)/f32_transform.manifest.json
  SHADER_BUNDLE_PREREQUISITE := $(PREBUILT_SHADER_DIR)/bundle.manifest.json
  SHADER_BUNDLE_VALIDATION := python3 tests/validate_prebuilt_shader_bundle.py \
		$(PREBUILT_SHADER_DIR) --source-root .
else
  $(error SHADER_MODE must be compile or prebuilt)
endif
COMPUTE_TEST := $(BIN_DIR)/vk_runtime_compute_live_test
COMPUTE_REPORT := $(BUILD_DIR)/compute_report.json
COMPUTE_REPEAT_REPORT := $(BUILD_DIR)/compute_report_repeat.json
COMPUTE_VALIDATION_REPORT := $(BUILD_DIR)/compute_report_validation.json
RESIDENT_TEST := $(BIN_DIR)/vk_runtime_resident_live_test
RESIDENT_REPORT := $(BUILD_DIR)/residency_report.json
RESIDENT_REPEAT_REPORT := $(BUILD_DIR)/residency_report_repeat.json
RESIDENT_VALIDATION_REPORT := $(BUILD_DIR)/residency_report_validation.json
TIMING_TEST := $(BIN_DIR)/vk_runtime_timing_live_test
TIMING_REPORT := $(BUILD_DIR)/timing_report.json
TIMING_REPEAT_REPORT := $(BUILD_DIR)/timing_report_repeat.json
TIMING_VALIDATION_REPORT := $(BUILD_DIR)/timing_report_validation.json

UNAME_S := $(shell uname -s)
VALIDATION_LAYER_DIR ?= $(patsubst %/,%,$(dir $(firstword \
	$(wildcard /opt/homebrew/lib/libVkLayer_khronos_validation.dylib) \
	$(wildcard /usr/local/lib/libVkLayer_khronos_validation.dylib))))
ifeq ($(UNAME_S),Darwin)
  VALIDATION_ENV := $(if $(VALIDATION_LAYER_DIR),DYLD_LIBRARY_PATH=$(VALIDATION_LAYER_DIR),)
else
  VALIDATION_ENV :=
endif

.PHONY: all shaders test test-shaders test-sanitize test-live \
	test-live-validation test-compute-live test-compute-live-validation \
	test-resident-live test-resident-live-validation \
	test-timing-live test-timing-live-validation \
	export-prebuilt-shaders test-prebuilt-contract \
	test-prebuilt-shaders test-prebuilt-live \
	probe clean print-flags

all: $(LIB) $(PROBE)

$(OBJ_DIR) $(LIB_DIR) $(BIN_DIR):
	mkdir -p $@

$(COMPILED_SHADER_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: src/%.c include/vk_runtime.h src/vk_runtime_internal.h \
		src/vk_runtime_resident_internal.h VERSION | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(PROBE): tools/vk_runtime_probe.c $(LIB) include/vk_runtime.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

$(CONTRACT_TEST): tests/test_vk_runtime_contract.c $(LIB) include/vk_runtime.h \
		tests/fixtures/capability_report_v1.json | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-DVK_RUNTIME_TEST_FIXTURE_PATH=\"$(CURDIR)/tests/fixtures/capability_report_v1.json\" \
		$< $(LIB) $(LDFLAGS) -o $@

$(COMPILED_U32_SPV): shaders/u32_transform.comp | $(COMPILED_SHADER_DIR)
	$(GLSLC) $(SHADER_FLAGS) $< -o $@

$(COMPILED_F32_SPV): shaders/f32_transform.comp | $(COMPILED_SHADER_DIR)
	$(GLSLC) $(SHADER_FLAGS) $< -o $@

$(COMPILED_U32_MANIFEST): $(COMPILED_U32_SPV) shaders/u32_transform.comp \
		tools/generate_shader_manifest.py
	python3 tools/generate_shader_manifest.py \
		--kernel-id codework_u32_transform_v1 \
		--source shaders/u32_transform.comp --spirv $(COMPILED_U32_SPV) \
		--output $@ --compiler $(GLSLC) \
		--flag=--target-env=vulkan1.1 --flag=-O

$(COMPILED_F32_MANIFEST): $(COMPILED_F32_SPV) shaders/f32_transform.comp \
		tools/generate_shader_manifest.py
	python3 tools/generate_shader_manifest.py \
		--kernel-id codework_f32_transform_v1 \
		--source shaders/f32_transform.comp --spirv $(COMPILED_F32_SPV) \
		--output $@ --compiler $(GLSLC) \
		--flag=--target-env=vulkan1.1 --flag=-O

$(COMPUTE_TEST): tests/test_vk_runtime_compute_live.c $(LIB) \
		include/vk_runtime.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDFLAGS) -lm -o $@

$(RESIDENT_TEST): tests/test_vk_runtime_resident_live.c $(LIB) \
		include/vk_runtime.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

$(TIMING_TEST): tests/test_vk_runtime_timing_live.c $(LIB) \
		include/vk_runtime.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

shaders: $(U32_MANIFEST) $(F32_MANIFEST) $(SHADER_BUNDLE_PREREQUISITE)

test-shaders: shaders
	python3 tests/validate_shader_manifest.py $(U32_MANIFEST) \
		--source shaders/u32_transform.comp --spirv $(U32_SPV) \
		--kernel-id codework_u32_transform_v1 --require-source-sha256
	python3 tests/validate_shader_manifest.py $(F32_MANIFEST) \
		--source shaders/f32_transform.comp --spirv $(F32_SPV) \
		--kernel-id codework_f32_transform_v1 --require-source-sha256
	$(SHADER_BUNDLE_VALIDATION)

export-prebuilt-shaders: $(COMPILED_U32_MANIFEST) $(COMPILED_F32_MANIFEST)
	python3 tools/export_prebuilt_shaders.py \
		--compiled-dir $(COMPILED_SHADER_DIR) \
		--output-dir $(PREBUILT_EXPORT_DIR)
	python3 tests/validate_prebuilt_shader_bundle.py \
		$(PREBUILT_EXPORT_DIR) --source-root .

test-prebuilt-contract: export-prebuilt-shaders
	python3 tests/test_prebuilt_shader_bundle_contract.py \
		--bundle-dir $(PREBUILT_EXPORT_DIR) --source-root .

test-prebuilt-shaders: test-prebuilt-contract
	$(MAKE) clean
	$(MAKE) SHADER_MODE=prebuilt \
		PREBUILT_SHADER_DIR=$(abspath $(PREBUILT_EXPORT_DIR)) test-shaders

test-prebuilt-live: export-prebuilt-shaders
	$(MAKE) clean
	$(MAKE) SHADER_MODE=prebuilt \
		PREBUILT_SHADER_DIR=$(abspath $(PREBUILT_EXPORT_DIR)) \
		test-compute-live test-resident-live test-timing-live

test: $(CONTRACT_TEST)
	./$(CONTRACT_TEST)

test-sanitize: | $(BIN_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Werror -Wpedantic -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-DVK_RUNTIME_TEST_FIXTURE_PATH=\"$(CURDIR)/tests/fixtures/capability_report_v1.json\" \
		tests/test_vk_runtime_contract.c $(SOURCES) $(LDFLAGS) -o $(SANITIZE_TEST)
	./$(SANITIZE_TEST)

probe: $(PROBE)
	./$(PROBE) --validate --pretty

test-live: $(PROBE)
	./$(PROBE) --pretty --output $(LIVE_REPORT)
	./$(PROBE) --pretty --output $(LIVE_REPEAT_REPORT)
	cmp $(LIVE_REPORT) $(LIVE_REPEAT_REPORT)
	python3 tests/validate_capability_report.py $(LIVE_REPORT)
	@grep -q '"schema": "codework_gpu_capability_report_v1"' $(LIVE_REPORT)
	@grep -q '"status": "ok"' $(LIVE_REPORT)
	@grep -q '"selected": true' $(LIVE_REPORT)

test-live-validation: $(PROBE)
	$(VALIDATION_ENV) ./$(PROBE) --require-validation \
		--fail-on-validation-error --pretty --output $(VALIDATION_REPORT)
	python3 tests/validate_capability_report.py \
		--require-validation $(VALIDATION_REPORT)
	@grep -q '"enabled": true' $(VALIDATION_REPORT)
	@grep -q '"load_failed": false' $(VALIDATION_REPORT)
	@grep -q '"errors": 0' $(VALIDATION_REPORT)
	@grep -q '"status": "ok"' $(VALIDATION_REPORT)

test-compute-live: $(COMPUTE_TEST) test-shaders
	U32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(U32_MANIFEST)); \
	F32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(F32_MANIFEST)); \
	./$(COMPUTE_TEST) --u32 $(U32_SPV) --f32 $(F32_SPV) \
		--u32-sha256 $$U32_SHA --f32-sha256 $$F32_SHA \
		--output $(COMPUTE_REPORT) && \
	./$(COMPUTE_TEST) --u32 $(U32_SPV) --f32 $(F32_SPV) \
		--u32-sha256 $$U32_SHA --f32-sha256 $$F32_SHA \
		--output $(COMPUTE_REPEAT_REPORT)
	python3 tests/compare_compute_reports.py $(COMPUTE_REPORT) \
		--compare $(COMPUTE_REPEAT_REPORT)

test-compute-live-validation: $(COMPUTE_TEST) test-shaders
	U32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(U32_MANIFEST)); \
	F32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(F32_MANIFEST)); \
	$(VALIDATION_ENV) ./$(COMPUTE_TEST) --require-validation \
		--u32 $(U32_SPV) --f32 $(F32_SPV) \
		--u32-sha256 $$U32_SHA --f32-sha256 $$F32_SHA \
		--output $(COMPUTE_VALIDATION_REPORT)
	python3 tests/compare_compute_reports.py \
		$(COMPUTE_VALIDATION_REPORT)

test-resident-live: $(RESIDENT_TEST) test-shaders
	U32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(U32_MANIFEST)); \
	./$(RESIDENT_TEST) --u32 $(U32_SPV) --u32-sha256 $$U32_SHA \
		--output $(RESIDENT_REPORT) && \
	./$(RESIDENT_TEST) --u32 $(U32_SPV) --u32-sha256 $$U32_SHA \
		--output $(RESIDENT_REPEAT_REPORT)
	python3 tests/compare_residency_reports.py $(RESIDENT_REPORT) \
		--compare $(RESIDENT_REPEAT_REPORT)

test-resident-live-validation: $(RESIDENT_TEST) test-shaders
	U32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(U32_MANIFEST)); \
	$(VALIDATION_ENV) ./$(RESIDENT_TEST) --require-validation \
		--u32 $(U32_SPV) --u32-sha256 $$U32_SHA \
		--output $(RESIDENT_VALIDATION_REPORT)
	python3 tests/compare_residency_reports.py \
		$(RESIDENT_VALIDATION_REPORT) --require-validation

test-timing-live: $(TIMING_TEST) test-shaders
	U32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(U32_MANIFEST)); \
	./$(TIMING_TEST) --u32 $(U32_SPV) --u32-sha256 $$U32_SHA \
		--output $(TIMING_REPORT) && \
	./$(TIMING_TEST) --u32 $(U32_SPV) --u32-sha256 $$U32_SHA \
		--output $(TIMING_REPEAT_REPORT)
	python3 tests/compare_timing_reports.py $(TIMING_REPORT) \
		--compare $(TIMING_REPEAT_REPORT)

test-timing-live-validation: $(TIMING_TEST) test-shaders
	U32_SHA=$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["spirv_sha256"])' $(U32_MANIFEST)); \
	$(VALIDATION_ENV) ./$(TIMING_TEST) --require-validation \
		--u32 $(U32_SPV) --u32-sha256 $$U32_SHA \
		--output $(TIMING_VALIDATION_REPORT)
	python3 tests/compare_timing_reports.py \
		$(TIMING_VALIDATION_REPORT) --require-validation

print-flags:
	@echo "CPPFLAGS = $(CPPFLAGS)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "SHADER_MODE = $(SHADER_MODE)"
	@echo "PREBUILT_SHADER_DIR = $(PREBUILT_SHADER_DIR)"
	@echo "VALIDATION_LAYER_DIR = $(VALIDATION_LAYER_DIR)"

clean:
	rm -rf $(BUILD_DIR)
