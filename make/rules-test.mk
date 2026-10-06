test run-headless-smoke run-headless-smoke-core run-headless-smoke-artifacts run-headless-smoke-continuity run-headless-smoke-throughput run-headless-smoke-full: BUILD_TOOLCHAIN := $(TEST_TOOLCHAIN)
test-%: BUILD_TOOLCHAIN := $(TEST_TOOLCHAIN)

run: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) MAPFORGE_VK_DEBUG=$(VK_DEBUG) MAPFORGE_REGIONS_DIR="$(MAPFORGE_REGIONS_DIR)" ./$(TARGET)

vulkan-rollout-contract:
	python3 tools/verify-vulkan-rollout.py --shared-root "$(SHARED_ROOT)"

vulkan-rollout-self-test: vulkan-rollout-contract
	rm -rf build
	$(MAKE) app
	mkdir -p build/vulkan-rollout
	MAPFORGE_RENDER_BACKEND=vulkan MAPFORGE_REQUIRE_VK_VALIDATION=1 MAPFORGE_VULKAN_ROLLOUT_INITIAL_CAPTURE=build/vulkan-rollout/initial.bmp MAPFORGE_VULKAN_ROLLOUT_RESIZED_CAPTURE=build/vulkan-rollout/resized.bmp DYLD_LIBRARY_PATH=$(VULKAN_VALIDATION_DYLD_PATH) ./$(TARGET) --vulkan-rollout-self-test
	python3 tools/verify-vulkan-rollout.py --shared-root "$(SHARED_ROOT)" --initial-capture build/vulkan-rollout/initial.bmp --resized-capture build/vulkan-rollout/resized.bmp

run-headless-smoke: run-headless-smoke-core

run-headless-smoke-core: app test-worker-contract test-route-service test-route-preview test-headless-playback test-presentation-stability test-polygon-cache-guardrails test-input-policy test-tile-manager-residency test-region-validate-strict test-region-validate-contract test-region-publish-safety test-runtime-source-policy test-archive-metrics-rollup test-coverage-metadata-contract
	@echo "map_forge core headless smoke passed (non-interactive)"

run-headless-smoke-artifacts: app test-headless-route-job test-headless-route-job-bundle test-headless-runtime-pins test-headless-saved-pin-script test-headless-saved-pin-skill-contract test-headless-route-frames test-headless-route-video-helper test-headless-visualizer-drop-stage test-upload-visualizer-drop-preflight test-headless-saved-pin-visualizer-publish-plan test-headless-saved-pin-visualizer-publish-wrapper
	@echo "map_forge artifact headless smoke passed"

run-headless-smoke-continuity: app test-route-preview-contract
	@echo "map_forge continuity headless smoke passed"

run-headless-smoke-throughput: app test-phase-d-throughput
	@echo "map_forge throughput headless smoke passed"

run-headless-smoke-full: run-headless-smoke-core run-headless-smoke-artifacts run-headless-smoke-continuity run-headless-smoke-throughput
	@echo "map_forge full headless smoke passed"

test-r5-callable: test-route-preview test-headless-playback test-tile-source-archive test-runtime-source-policy test-archive-metrics-rollup test-headless-saved-pin-visualizer-publish-plan
	@echo "map_forge R5 callable probes passed"

visual-harness: app
	@echo "visual harness binary ready: $(TARGET)"

visual-artifact: app
	@artifact_root="visual_artifacts/source_run_first_frame"; \
	run_dir="$$artifact_root/route_demo_seattle"; \
	preview_path="$$run_dir/preview.bmp"; \
	rm -rf "$$run_dir"; \
	mkdir -p "$$run_dir"; \
	"$(APP_BIN)" --headless --job "jobs/examples/route_demo_seattle.json" --out "$$run_dir" >/dev/null; \
	test -s "$$preview_path" || (echo "visual-artifact failed: missing or empty $$preview_path" >&2; exit 1); \
	echo "visual-artifact=$$preview_path"

carta-local-proof:
	@$(MAKE) --no-print-directory test-r5-callable
	@$(MAKE) --no-print-directory visual-artifact
	@$(MAKE) --no-print-directory run-headless-smoke-core
	@$(MAKE) --no-print-directory test-headless-saved-pin-visualizer-publish-wrapper
	@$(MAKE) --no-print-directory package-desktop-self-test
	@echo "carta-local-proof passed"

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

test: test-space build-safety-check main-edit-package-contract-checks
test: test-region-validate-strict
test: test-region-validate-contract
test: test-region-publish-safety
test: test-runtime-source-policy
test: test-archive-metrics-rollup
test: test-coverage-metadata-contract
test: test-trace-contract
test: test-worker-contract
test: test-tile-loader-shutdown
test: test-tile-source-archive
test: test-mft-loader-security
test: test-route-service
test: test-route-preview-contract
test: test-tile-presenter-policy
test: test-presentation-stability
test: test-input-policy
test: test-map-viewport
test: test-pin-panel-layout
test: test-pin-panel-rename
test: test-tile-manager-residency
test: test-pins
test: test-phase-d-throughput

test-region-validate-strict: tools-build
	./tests/test_region_validate_strict.sh

test-region-validate-contract: tools-build
	./tests/test_region_validate_contract.sh

test-region-publish-safety: tools-build
	./tests/test_region_publish_safety.sh

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

test-headless-failure-diagnostics: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_failure_diagnostics.sh

test-headless-route-job-bundle: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_route_job_bundle.sh

test-headless-runtime-pins: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_runtime_pins.sh

test-headless-saved-pin-script: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_saved_pin_route_script.sh

test-headless-saved-pin-skill-contract: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_saved_pin_skill_contract.sh

test-headless-route-frames: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_route_frames.sh

test-headless-route-video-helper: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_route_video_helper.sh

test-headless-visualizer-drop-stage: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_visualizer_drop_stage.sh

test-upload-visualizer-drop-preflight:
	/bin/sh ./tests/test_upload_visualizer_drop_preflight.sh

test-headless-saved-pin-visualizer-publish-plan:
	/bin/sh ./tests/test_headless_saved_pin_visualizer_publish_plan.sh

test-headless-saved-pin-visualizer-publish-wrapper: app
	MAPFORGE_BINARY="$(TEST_APP_BIN)" /bin/sh ./tests/test_headless_saved_pin_visualizer_publish_wrapper.sh

test-route-preview-contract: test-route-preview test-phase-b-continuity-stress
	@echo "map_forge route preview contract lane passed"

test-route-preview: $(APP_ROUTE_PREVIEW_TEST_TARGET)
	./$(APP_ROUTE_PREVIEW_TEST_TARGET)

test-tile-presenter-policy: $(APP_TILE_PRESENTER_POLICY_TEST_TARGET)
	./$(APP_TILE_PRESENTER_POLICY_TEST_TARGET)

test-polygon-fill-gate: $(APP_POLYGON_FILL_GATE_TEST_TARGET)
	./$(APP_POLYGON_FILL_GATE_TEST_TARGET)

test-vk-polygon-fill-geometry-guard: $(VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_TARGET)
	./$(VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_TARGET)

test-presentation-stability: $(APP_TILE_PRESENTER_POLICY_TEST_TARGET) $(APP_POLYGON_FILL_GATE_TEST_TARGET) $(VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_TARGET)
	./$(APP_TILE_PRESENTER_POLICY_TEST_TARGET)
	./$(APP_POLYGON_FILL_GATE_TEST_TARGET)
	./$(VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_TARGET)

test-polygon-cache-guardrails: $(POLYGON_CACHE_GUARDRAILS_TEST_TARGET)
	./$(POLYGON_CACHE_GUARDRAILS_TEST_TARGET)

test-mft-loader-security: $(MFT_LOADER_SECURITY_TEST_TARGET)
	./$(MFT_LOADER_SECURITY_TEST_TARGET)

test-input-policy: $(APP_RUNTIME_INPUT_POLICY_TEST_TARGET)
	./$(APP_RUNTIME_INPUT_POLICY_TEST_TARGET)

test-workspace-authoring-host: $(MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_TARGET)
	./$(MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_TARGET)

test-header-layer-layout: $(APP_HEADER_LAYER_LAYOUT_TEST_TARGET)
	./$(APP_HEADER_LAYER_LAYOUT_TEST_TARGET)

test-window-resize: $(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET)
	./$(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET)

test-map-viewport: $(APP_MAP_VIEWPORT_TEST_TARGET)
	./$(APP_MAP_VIEWPORT_TEST_TARGET)

test-pin-panel-layout: $(APP_PIN_PANEL_LAYOUT_TEST_TARGET)
	./$(APP_PIN_PANEL_LAYOUT_TEST_TARGET)

test-pin-panel-rename: $(APP_PIN_PANEL_RENAME_TEST_TARGET)
	./$(APP_PIN_PANEL_RENAME_TEST_TARGET)

test-tile-manager-residency: $(TILE_MANAGER_RESIDENCY_TEST_TARGET)
	./$(TILE_MANAGER_RESIDENCY_TEST_TARGET)

test-pins: $(APP_PINS_TEST_TARGET)
	./$(APP_PINS_TEST_TARGET)

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

$(APP_POLYGON_FILL_GATE_TEST_TARGET): $(APP_POLYGON_FILL_GATE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_POLYGON_FILL_GATE_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_TARGET): $(VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude -Isrc $(VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(POLYGON_CACHE_GUARDRAILS_TEST_TARGET): $(POLYGON_CACHE_GUARDRAILS_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(POLYGON_CACHE_GUARDRAILS_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(MFT_LOADER_SECURITY_TEST_TARGET): $(MFT_LOADER_SECURITY_TEST_SRCS) $(CORE_IO_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(MFT_LOADER_SECURITY_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_IO_LIB) $(CORE_BASE_LIB)

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

$(APP_MAP_VIEWPORT_TEST_TARGET): $(APP_MAP_VIEWPORT_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_MAP_VIEWPORT_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_SHARED_LIBS)

$(APP_PIN_PANEL_LAYOUT_TEST_TARGET): $(APP_PIN_PANEL_LAYOUT_TEST_SRCS) $(CORE_PANE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_PIN_PANEL_LAYOUT_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(JSON_LIBS) $(CORE_SHARED_LIBS)

$(APP_PIN_PANEL_RENAME_TEST_TARGET): $(APP_PIN_PANEL_RENAME_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_PIN_PANEL_RENAME_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(TILE_MANAGER_RESIDENCY_TEST_TARGET): $(TILE_MANAGER_RESIDENCY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TILE_MANAGER_RESIDENCY_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_SHARED_LIBS)

$(APP_PINS_TEST_TARGET): $(APP_PINS_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_PINS_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(JSON_LIBS)

test: test-route-cost

test-route-cost: $(ROUTE_COST_TEST_TARGET)
	./$(ROUTE_COST_TEST_TARGET)

$(ROUTE_COST_TEST_TARGET): $(ROUTE_COST_TEST_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(ROUTE_COST_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_IO_LIB) $(CORE_BASE_LIB)

test: test-utility-workspace

test-utility-workspace: $(UTILITY_WORKSPACE_TEST_TARGET)
	./$(UTILITY_WORKSPACE_TEST_TARGET)

$(UTILITY_WORKSPACE_TEST_TARGET): $(UTILITY_WORKSPACE_TEST_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(UTILITY_WORKSPACE_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(JSON_LIBS) $(SDL_LIBS) $(CORE_THEME_LIB) $(CORE_FONT_LIB)

test: test-utility-pack

test-utility-pack: tools-build graph-build
	python3 tests/test_utility_pack.py --tools-dir "$(TOOL_BIN_DIR)"

test-place-intake: $(PLACES_TARGET)
	python3 tests/test_place_intake.py $(PLACES_TARGET)
test: test-place-intake
test-route-query: $(ROUTE_QUERY_TEST_TARGET)
	./$(ROUTE_QUERY_TEST_TARGET)
$(ROUTE_QUERY_TEST_TARGET): $(ROUTE_QUERY_TEST_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(ROUTE_QUERY_TEST_SRCS) -o $@ $(TOOL_LDLIBS)
test: test-route-query

test-gpx-preview:
	python3 tests/test_gpx_preview.py
test: test-gpx-preview

test-place-tools: $(PLACE_TOOLS_TEST_TARGET)
	./$(PLACE_TOOLS_TEST_TARGET)
$(PLACE_TOOLS_TEST_TARGET): $(PLACE_TOOLS_TEST_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(PLACE_TOOLS_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(JSON_LIBS) $(SDL_LIBS) $(CORE_THEME_LIB) $(CORE_FONT_LIB)
test: test-place-tools

test: test-region-band-selection
.PHONY: test-region-band-selection
test-region-band-selection: $(REGION_BAND_SELECTION_TEST_TARGET)
	./$(REGION_BAND_SELECTION_TEST_TARGET)

$(REGION_BAND_SELECTION_TEST_TARGET): $(REGION_BAND_SELECTION_TEST_SRCS) include/app/region.h
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(REGION_BAND_SELECTION_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

.PHONY: test-tile-submission-budget test-region-prepare
test: test-tile-submission-budget test-region-prepare
test-tile-submission-budget: $(TILE_SUBMISSION_BUDGET_TEST_TARGET)
	./$(TILE_SUBMISSION_BUDGET_TEST_TARGET)
$(TILE_SUBMISSION_BUDGET_TEST_TARGET): $(TILE_SUBMISSION_BUDGET_TEST_SRCS) include/app/app_tile_pipeline_helpers.h
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TILE_SUBMISSION_BUDGET_TEST_SRCS) -o $@ $(TOOL_LDLIBS)
test-region-prepare: $(REGION_PREPARE_TEST_TARGET)
	./$(REGION_PREPARE_TEST_TARGET)
$(REGION_PREPARE_TEST_TARGET): $(REGION_PREPARE_TEST_SRCS) include/app/region_prepare.h include/app/app_internal.h include/app/app_region_transition.h include/map/polygon_cache.h include/map/polygon_triangulator.h $(CORE_SHARED_LIBS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(REGION_PREPARE_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(JSON_LIBS) $(CORE_SHARED_LIBS)

# Exercise the durable hiking command boundary using disposable repositories.
test-history: $(PLACES_TARGET)
	python3 tests/test_history.py $(PLACES_TARGET)
test: test-history

# Check native history events and command emission without a personal workspace.
test-history-ui: $(HISTORY_UI_TEST_TARGET)
	./$(HISTORY_UI_TEST_TARGET)
$(HISTORY_UI_TEST_TARGET): $(HISTORY_UI_TEST_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(HISTORY_UI_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(JSON_LIBS) $(SDL_LIBS) $(CORE_THEME_LIB) $(CORE_FONT_LIB)
test: test-history-ui

.PHONY: test-terrain test-terrain-prepare
test-terrain: $(TEST_BIN_DIR)/test_terrain_pack
	python3 tests/test_terrain_pack.py $(TEST_BIN_DIR)/test_terrain_pack
$(TEST_BIN_DIR)/test_terrain_pack: tests/test_terrain_pack.c src/map/terrain_pack.c include/map/terrain_pack.h
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude tests/test_terrain_pack.c src/map/terrain_pack.c -o $@ $(SQLITE_LIBS) $(JSON_LIBS) -lz -lm
TERRAIN_PREPARE_PYTHON ?= python3
test-terrain-prepare:
	$(TERRAIN_PREPARE_PYTHON) tests/test_terrain_prepare.py $(TERRAIN_SOURCE_DIR)
test: test-terrain

.PHONY: test-terrain-draw
test-terrain-draw: $(TEST_BIN_DIR)/test_terrain_draw
	./$(TEST_BIN_DIR)/test_terrain_draw
$(TEST_BIN_DIR)/test_terrain_draw: tests/test_terrain_draw.c src/app/terrain.c src/app/terrain_decode.c src/map/terrain_catalog.c include/map/terrain_pack.h $(CORE_IO_LIB) $(CORE_BASE_LIB) $(CORE_WORKERS_LIB) $(CORE_QUEUE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude tests/test_terrain_draw.c src/app/terrain.c src/app/terrain_decode.c src/map/terrain_catalog.c -o $@ $(CORE_WORKERS_LIB) $(CORE_QUEUE_LIB) $(CORE_IO_LIB) $(CORE_BASE_LIB) $(SDL_LIBS) $(JSON_LIBS) -lm
test: test-terrain-draw

.PHONY: test-terrain-vectors
test-terrain-vectors: $(TEST_BIN_DIR)/test_terrain_vectors
	$(TERRAIN_PREPARE_PYTHON) tests/test_terrain_vectors.py $(TEST_BIN_DIR)/test_terrain_vectors
$(TEST_BIN_DIR)/test_terrain_vectors: tests/test_terrain_vectors.c src/map/terrain_vectors.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $^ -o $@ $(JSON_LIBS) -lsqlite3 -lz -lm
test: test-terrain-vectors

# Verify independent hiking progress and named list reconciliation with disposable stores.
test-history-status: $(PLACES_TARGET)
	python3 tests/test_history_status.py $(PLACES_TARGET)
test: test-history-status

# Verify idle dispatch and asynchronous wake policy without presenting a native frame.
test-idle-runtime: $(TEST_BIN_DIR)/app_runtime_idle_test
	./$(TEST_BIN_DIR)/app_runtime_idle_test

test: test-idle-runtime

$(TEST_BIN_DIR)/app_runtime_idle_test: tests/app_runtime_idle_test.c src/app/app_runtime_wait_policy.c src/app/app_runtime_dispatch.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $^ third_party/codework_shared/core/core_queue/src/core_queue.c -o $@ $(TOOL_LDLIBS)

.PHONY: test-terrain-catalog
test-terrain-catalog: $(TEST_BIN_DIR)/test_terrain_catalog
	python3 tests/test_terrain_catalog.py $(TEST_BIN_DIR)/test_terrain_catalog
$(TEST_BIN_DIR)/test_terrain_catalog: tests/test_terrain_catalog.c src/map/terrain_catalog.c src/map/terrain_pack.c src/map/terrain_vectors.c $(CORE_IO_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude tests/test_terrain_catalog.c src/map/terrain_catalog.c src/map/terrain_pack.c src/map/terrain_vectors.c -o $@ $(CORE_IO_LIB) $(CORE_BASE_LIB) $(SQLITE_LIBS) $(JSON_LIBS) -lz -lm
test: test-terrain-catalog

.PHONY: test-terrain-decode
test-terrain-decode: $(TEST_BIN_DIR)/test_terrain_decode
	./$(TEST_BIN_DIR)/test_terrain_decode
$(TEST_BIN_DIR)/test_terrain_decode: tests/test_terrain_decode.c src/app/terrain_decode.c include/app/terrain_decode.h $(CORE_WORKERS_LIB) $(CORE_QUEUE_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BIN_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude tests/test_terrain_decode.c src/app/terrain_decode.c -o $@ $(CORE_WORKERS_LIB) $(CORE_QUEUE_LIB) $(CORE_BASE_LIB) $(SDL_LIBS)
test: test-terrain-decode

.PHONY: test-terrain-presentation
test-terrain-presentation: $(TEST_BIN_DIR)/test_terrain_presentation
	./$(TEST_BIN_DIR)/test_terrain_presentation
$(TEST_BIN_DIR)/test_terrain_presentation: tests/test_terrain_presentation.c src/app/terrain.c src/app/terrain_decode.c include/app/terrain.h $(CORE_WORKERS_LIB) $(CORE_QUEUE_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BIN_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude tests/test_terrain_presentation.c src/app/terrain.c src/app/terrain_decode.c -o $@ $(CORE_WORKERS_LIB) $(CORE_QUEUE_LIB) $(CORE_BASE_LIB) $(SDL_LIBS) -lm
test: test-terrain-presentation

.PHONY: test-terrain-vector-presentation
test-terrain-vector-presentation: $(TEST_BIN_DIR)/test_terrain_vector_presentation
	./$(TEST_BIN_DIR)/test_terrain_vector_presentation
$(TEST_BIN_DIR)/test_terrain_vector_presentation: tests/test_terrain_vector_presentation.c src/app/terrain_vectors.c include/app/terrain_vectors.h
	@mkdir -p $(TEST_BIN_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude tests/test_terrain_vector_presentation.c src/app/terrain_vectors.c -o $@ $(SDL_LIBS) $(JSON_LIBS) -lm
test: test-terrain-vector-presentation

.PHONY: test-carta-text
$(CARTA_TEXT_TEST_TARGET): $(CARTA_TEXT_INPUT_SRCS) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(CARTA_TEXT_INPUT_SRCS) -o $@ $(SDL_LIBS) $(CORE_BASE_LIB)
test-carta-text: $(CARTA_TEXT_TEST_TARGET)
	./$(CARTA_TEXT_TEST_TARGET)
test: test-carta-text
