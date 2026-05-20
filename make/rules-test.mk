test run-headless-smoke: BUILD_TOOLCHAIN := $(TEST_TOOLCHAIN)
test-%: BUILD_TOOLCHAIN := $(TEST_TOOLCHAIN)

run: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) MAPFORGE_VK_DEBUG=$(VK_DEBUG) MAPFORGE_REGIONS_DIR="$(MAPFORGE_REGIONS_DIR)" ./$(TARGET)

run-headless-smoke: app test-worker-contract test-route-service test-headless-playback test-headless-route-job test-headless-route-frames test-presentation-stability test-polygon-cache-guardrails test-input-policy test-tile-manager-residency test-phase-d-throughput test-region-validate-strict test-region-validate-contract test-runtime-source-policy test-archive-metrics-rollup test-coverage-metadata-contract
	@echo "map_forge headless smoke passed (non-interactive)"

visual-harness: app
	@echo "visual harness binary ready: $(TARGET)"

run-ide-theme: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) MAPFORGE_VK_DEBUG=$(VK_DEBUG) \
	MAPFORGE_REGIONS_DIR="$(MAPFORGE_REGIONS_DIR)" \
	MAPFORGE_USE_SHARED_THEME_FONT=1 MAPFORGE_USE_SHARED_THEME=1 MAPFORGE_USE_SHARED_FONT=1 \
	MAPFORGE_THEME_PRESET=ide_gray MAPFORGE_FONT_PRESET=ide ./$(TARGET)

run-daw-theme: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) MAPFORGE_VK_DEBUG=$(VK_DEBUG) \
	MAPFORGE_REGIONS_DIR="$(MAPFORGE_REGIONS_DIR)" \
	MAPFORGE_USE_SHARED_THEME_FONT=1 MAPFORGE_USE_SHARED_THEME=1 MAPFORGE_USE_SHARED_FONT=1 \
	MAPFORGE_THEME_PRESET=daw_default MAPFORGE_FONT_PRESET=daw_default ./$(TARGET)

test-space: $(MAP_SPACE_TEST_TARGET)
	./$(MAP_SPACE_TEST_TARGET)

build-safety-check: tools graph
	./tests/test_build_safety.sh

test: test-space build-safety-check
test: test-region-validate-strict
test: test-region-validate-contract
test: test-runtime-source-policy
test: test-archive-metrics-rollup
test: test-coverage-metadata-contract
test: test-trace-contract
test: test-worker-contract
test: test-tile-loader-shutdown
test: test-tile-source-archive
test: test-route-service
test: test-tile-presenter-policy
test: test-presentation-stability
test: test-input-policy
test: test-tile-manager-residency
test: test-phase-d-throughput

test-region-validate-strict: tools-build
	./tests/test_region_validate_strict.sh

test-region-validate-contract: tools-build
	./tests/test_region_validate_contract.sh

test-runtime-source-policy: tools-build
	./tests/test_runtime_source_policy.sh

test-archive-metrics-rollup: tools-build
	./tests/test_archive_metrics_rollup.sh

test-coverage-metadata-contract: tools-build
	./tests/test_coverage_metadata_contract.sh

metrics-rollup-gate: test-archive-metrics-rollup
	@echo "metrics-rollup-gate passed"

test-shared-theme-font-adapter: $(SHARED_THEME_FONT_ADAPTER_TEST_TARGET)
	./$(SHARED_THEME_FONT_ADAPTER_TEST_TARGET)

test-trace-contract: $(MAP_TRACE_CONTRACT_TEST_TARGET)
	./$(MAP_TRACE_CONTRACT_TEST_TARGET)

test-worker-contract: $(APP_WORKER_CONTRACT_TEST_TARGET)
	./$(APP_WORKER_CONTRACT_TEST_TARGET)

test-tile-loader-shutdown: $(TILE_LOADER_SHUTDOWN_TEST_TARGET)
	./$(TILE_LOADER_SHUTDOWN_TEST_TARGET)

test-tile-source-archive: $(TILE_SOURCE_ARCHIVE_TEST_TARGET)
	./$(TILE_SOURCE_ARCHIVE_TEST_TARGET)

test-route-service: $(APP_ROUTE_SERVICE_TEST_TARGET)
	./$(APP_ROUTE_SERVICE_TEST_TARGET)

test-headless-playback: $(APP_HEADLESS_PLAYBACK_TEST_TARGET)
	./$(APP_HEADLESS_PLAYBACK_TEST_TARGET)

test-headless-route-job: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_route_job.sh

test-headless-route-frames: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_route_frames.sh

test-route-preview: $(APP_ROUTE_PREVIEW_TEST_TARGET)
	./$(APP_ROUTE_PREVIEW_TEST_TARGET)

test-tile-presenter-policy: $(APP_TILE_PRESENTER_POLICY_TEST_TARGET)
	./$(APP_TILE_PRESENTER_POLICY_TEST_TARGET)

test-presentation-stability: $(APP_TILE_PRESENTER_POLICY_TEST_TARGET)
	./$(APP_TILE_PRESENTER_POLICY_TEST_TARGET)

test-polygon-cache-guardrails: $(POLYGON_CACHE_GUARDRAILS_TEST_TARGET)
	./$(POLYGON_CACHE_GUARDRAILS_TEST_TARGET)

test-input-policy: $(APP_RUNTIME_INPUT_POLICY_TEST_TARGET)
	./$(APP_RUNTIME_INPUT_POLICY_TEST_TARGET)

test-workspace-authoring-host: $(MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_TARGET)
	./$(MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_TARGET)

test-header-layer-layout: $(APP_HEADER_LAYER_LAYOUT_TEST_TARGET)
	./$(APP_HEADER_LAYER_LAYOUT_TEST_TARGET)

test-window-resize: $(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET)
	./$(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET)

test-tile-manager-residency: $(TILE_MANAGER_RESIDENCY_TEST_TARGET)
	./$(TILE_MANAGER_RESIDENCY_TEST_TARGET)

test-phase-a-viewport-scenario: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" MAPFORGE_PHASE_A_MAX_L0_LATENCY_MS=1500 \
	./tests/test_phase_a_viewport_scenario.sh

test-phase-b-continuity-stress: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" ./tests/test_phase_b_continuity_stress.sh

test-phase-d1-budget-control: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" MAPFORGE_PHASE_B_MAX_L0_LATENCY_MS=2500 \
	MAPFORGE_PHASE_D1_MAX_LOAD_CLAMP_TOTAL=48 \
	MAPFORGE_PHASE_D1_MAX_LOAD_EX_TOTAL=72 \
	MAPFORGE_PHASE_D1_MAX_L2_HITS_TOTAL=12 \
	MAPFORGE_PHASE_D1_MAX_L3_HITS_TOTAL=12 \
	MAPFORGE_PHASE_D1_MAX_INTEG_CLAMP_TOTAL=16 \
	MAPFORGE_PHASE_D1_MAX_INTEG_EX_TOTAL=32 \
	MAPFORGE_PHASE_D1_MAX_VK_ASSET_SAT_TOTAL=8 \
	MAPFORGE_PHASE_D1_MAX_VK_POLY_HIT_TOTAL=8 \
	./tests/test_phase_b_continuity_stress.sh

test-phase-d1-budget-matrix: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" ./tests/test_phase_d1_budget_matrix.sh

test-phase-d2-trace-matrix: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" ./tests/test_phase_d2_trace_matrix.sh

test-phase-d2-tuning-profiles: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" ./tests/test_phase_d2_tuning_profiles.sh

test-phase-d2-trend-summary: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" MAPFORGE_PHASE_D2_SKIP_GUARDRAILS=1 \
	MAPFORGE_PHASE_D2_TREND_WINDOW=8 \
	./tests/test_phase_d2_tuning_profiles.sh

test-phase-d2-guardrails: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_SEATTLE=100000 \
	MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_SEATTLE=100000 \
	MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_DOWNTOWN=100000 \
	MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_DOWNTOWN=100000 \
	MAPFORGE_PHASE_D2_MAX_MIN_COV_DROP=0.010 \
	MAPFORGE_PHASE_D2_MAX_FB_RATIO_DELTA=0.001 \
	MAPFORGE_PHASE_D2_MAX_CHURN_BAND_DELTA=0 \
	MAPFORGE_PHASE_D2_MAX_CHURN_QUEUE_DELTA=0 \
	MAPFORGE_PHASE_D2_MAX_BAND_FB_PEAK_DELTA=0 \
	./tests/test_phase_d2_tuning_profiles.sh

test-phase-d3-regression-gate: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" MAPFORGE_PHASE_D_GATE_MODE=d3 \
	MAPFORGE_D2_BASELINE_PROFILE_NAME=baseline \
	MAPFORGE_D2_BASELINE_PRESET=baseline \
	MAPFORGE_D2_CANDIDATE_PROFILE_NAME=l0_relief_candidate \
	MAPFORGE_D2_CANDIDATE_PRESET=l0_relief_candidate \
	MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_SEATTLE=100000 \
	MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_DOWNTOWN=100000 \
	MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_SEATTLE=100000 \
	MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_DOWNTOWN=100000 \
	MAPFORGE_PHASE_D2_MAX_MIN_COV_DROP=1.000 \
	MAPFORGE_PHASE_D2_MAX_FB_RATIO_DELTA=0.001 \
	MAPFORGE_PHASE_D2_MAX_CHURN_BAND_DELTA=0 \
	MAPFORGE_PHASE_D2_MAX_CHURN_QUEUE_DELTA=0 \
	MAPFORGE_PHASE_D2_MAX_BAND_FB_PEAK_DELTA=0 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_LOAD_CLAMP_TOTAL=100000 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_LOAD_EX_TOTAL=100000 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_L2_HITS_TOTAL=100000 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_L3_HITS_TOTAL=100000 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_INTEG_CLAMP_TOTAL=100000 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_INTEG_EX_TOTAL=100000 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_VK_ASSET_SAT_TOTAL=100000 \
	MAPFORGE_PHASE_D2_MATRIX_MAX_VK_POLY_HIT_TOTAL=100000 \
	MAPFORGE_PHASE_D3_ALERT_LOAD_EX_DELTA_SEATTLE=8 \
	MAPFORGE_PHASE_D3_ALERT_L0_PEAK_DELTA_MS_SEATTLE=1200 \
	MAPFORGE_PHASE_D3_ALERT_LOAD_EX_DELTA_DOWNTOWN=10 \
	MAPFORGE_PHASE_D3_ALERT_L0_PEAK_DELTA_MS_DOWNTOWN=500 \
	./tests/test_phase_d2_tuning_profiles.sh

test-phase-d3-contract-preview: test-phase-d3-regression-gate

test-phase-d-throughput: app
	$(MAKE) --no-print-directory test-phase-d1-budget-control
	$(MAKE) --no-print-directory test-phase-d2-guardrails
	$(MAKE) --no-print-directory test-phase-d3-regression-gate

$(MAP_SPACE_TEST_TARGET): $(MAP_SPACE_TEST_SRCS) $(CORE_VIEWPORT2D_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(MAP_SPACE_TEST_SRCS) $(CORE_VIEWPORT2D_LIB) $(CORE_SPACE_LIB) $(CORE_BASE_LIB) -o $@ $(TOOL_LDLIBS)

$(SHARED_THEME_FONT_ADAPTER_TEST_TARGET): $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(MAP_TRACE_CONTRACT_TEST_TARGET): $(MAP_TRACE_CONTRACT_TEST_SRCS) $(CORE_TRACE_LIB) $(CORE_PACK_LIB) $(CORE_IO_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(MAP_TRACE_CONTRACT_TEST_SRCS) -o $@ $(CORE_TRACE_LIB) $(CORE_PACK_LIB) $(CORE_IO_LIB) $(CORE_BASE_LIB) -lm

$(APP_WORKER_CONTRACT_TEST_TARGET): $(APP_WORKER_CONTRACT_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_WORKER_CONTRACT_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(TILE_LOADER_SHUTDOWN_TEST_TARGET): $(TILE_LOADER_SHUTDOWN_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TILE_LOADER_SHUTDOWN_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_SHARED_LIBS)

$(TILE_SOURCE_ARCHIVE_TEST_TARGET): $(TILE_SOURCE_ARCHIVE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TILE_SOURCE_ARCHIVE_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_ROUTE_SERVICE_TEST_TARGET): $(APP_ROUTE_SERVICE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_ROUTE_SERVICE_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_HEADLESS_PLAYBACK_TEST_TARGET): $(APP_HEADLESS_PLAYBACK_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_HEADLESS_PLAYBACK_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_ROUTE_PREVIEW_TEST_TARGET): $(APP_ROUTE_PREVIEW_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_ROUTE_PREVIEW_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_TILE_PRESENTER_POLICY_TEST_TARGET): $(APP_TILE_PRESENTER_POLICY_TEST_SRCS) $(CORE_TIME_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_TILE_PRESENTER_POLICY_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_TIME_LIB)

$(POLYGON_CACHE_GUARDRAILS_TEST_TARGET): $(POLYGON_CACHE_GUARDRAILS_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(POLYGON_CACHE_GUARDRAILS_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_RUNTIME_INPUT_POLICY_TEST_TARGET): $(APP_RUNTIME_INPUT_POLICY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_RUNTIME_INPUT_POLICY_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_TARGET): $(MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_HEADER_LAYER_LAYOUT_TEST_TARGET): $(APP_HEADER_LAYER_LAYOUT_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_HEADER_LAYER_LAYOUT_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET): $(APP_RUNTIME_WINDOW_RESIZE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_RUNTIME_WINDOW_RESIZE_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(TILE_MANAGER_RESIDENCY_TEST_TARGET): $(TILE_MANAGER_RESIDENCY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TILE_MANAGER_RESIDENCY_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_SHARED_LIBS)
