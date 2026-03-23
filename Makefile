CC := cc

# Build the application when running plain `make`.
.DEFAULT_GOAL := app

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SDL_TTF_CFLAGS := $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell pkg-config --libs SDL2_ttf 2>/dev/null)
VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
JSON_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)
SHARED_ROOT ?= third_party/codework_shared
CORE_SPACE_DIR := $(SHARED_ROOT)/core/core_space
CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
CORE_TIME_DIR := $(SHARED_ROOT)/core/core_time
CORE_QUEUE_DIR := $(SHARED_ROOT)/core/core_queue
CORE_SCHED_DIR := $(SHARED_ROOT)/core/core_sched
CORE_JOBS_DIR := $(SHARED_ROOT)/core/core_jobs
CORE_WORKERS_DIR := $(SHARED_ROOT)/core/core_workers
CORE_WAKE_DIR := $(SHARED_ROOT)/core/core_wake
CORE_KERNEL_DIR := $(SHARED_ROOT)/core/core_kernel
CORE_TRACE_DIR := $(SHARED_ROOT)/core/core_trace
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font

CORE_SPACE_LIB := $(CORE_SPACE_DIR)/build/libcore_space.a
CORE_BASE_LIB := $(CORE_BASE_DIR)/build/libcore_base.a
CORE_IO_LIB := $(CORE_IO_DIR)/build/libcore_io.a
CORE_DATA_LIB := $(CORE_DATA_DIR)/build/libcore_data.a
CORE_PACK_LIB := $(CORE_PACK_DIR)/build/libcore_pack.a
CORE_TIME_LIB := $(CORE_TIME_DIR)/build/libcore_time.a
CORE_QUEUE_LIB := $(CORE_QUEUE_DIR)/build/libcore_queue.a
CORE_SCHED_LIB := $(CORE_SCHED_DIR)/build/libcore_sched.a
CORE_JOBS_LIB := $(CORE_JOBS_DIR)/build/libcore_jobs.a
CORE_WORKERS_LIB := $(CORE_WORKERS_DIR)/build/libcore_workers.a
CORE_WAKE_LIB := $(CORE_WAKE_DIR)/build/libcore_wake.a
CORE_KERNEL_LIB := $(CORE_KERNEL_DIR)/build/libcore_kernel.a
CORE_TRACE_LIB := $(CORE_TRACE_DIR)/build/libcore_trace.a
CORE_THEME_LIB := $(CORE_THEME_DIR)/build/libcore_theme.a
CORE_FONT_LIB := $(CORE_FONT_DIR)/build/libcore_font.a

VK_RENDERER_DIR ?= $(SHARED_ROOT)/vk_renderer
VK_RENDERER_RESOLVED_DIR := $(VK_RENDERER_DIR)
VK_RENDERER_INCLUDE := $(VK_RENDERER_RESOLVED_DIR)/include
VK_RENDERER_STATIC_LIB := $(VK_RENDERER_RESOLVED_DIR)/build/lib/libvkrenderer.a
VK_RENDERER_SRCS := $(wildcard $(VK_RENDERER_RESOLVED_DIR)/src/*.c)
VK_RENDERER_OBJS := $(patsubst $(VK_RENDERER_RESOLVED_DIR)/src/%.c,build/vk_renderer/%.o,$(VK_RENDERER_SRCS))
VK_BUILD_LIB := build/vk/lib/libvkrenderer.a
VK_BUILD_SHADER_DIR := build/vk/shaders
VK_REQUIRED_SHADERS := fill.vert.spv fill.frag.spv line.vert.spv line.frag.spv textured.vert.spv textured.frag.spv
VK_APP_ENABLED := $(if $(wildcard $(VK_RENDERER_INCLUDE)/vk_renderer.h),1,)

ifeq ($(SDL_LIBS),)
SDL_CFLAGS :=
SDL_LIBS := -lSDL2
endif

ifeq ($(SDL_TTF_LIBS),)
SDL_TTF_LIBS := -lSDL2_ttf
endif

ifeq ($(VULKAN_LIBS),)
VULKAN_CFLAGS :=
VULKAN_LIBS := -lvulkan
endif

CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2 -g -pthread $(SDL_CFLAGS) $(SDL_TTF_CFLAGS)
LDLIBS := $(SDL_LIBS) $(SDL_TTF_LIBS) $(JSON_LIBS) -pthread
TOOL_LDLIBS := -lm $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)

ifeq ($(JSON_LIBS),)
LDLIBS += -ljson-c
endif
CFLAGS += $(JSON_CFLAGS)
CFLAGS += -I$(CORE_SPACE_DIR)/include
CFLAGS += -I$(CORE_BASE_DIR)/include
CFLAGS += -I$(CORE_IO_DIR)/include
CFLAGS += -I$(CORE_DATA_DIR)/include
CFLAGS += -I$(CORE_PACK_DIR)/include
CFLAGS += -I$(CORE_TIME_DIR)/include
CFLAGS += -I$(CORE_QUEUE_DIR)/include
CFLAGS += -I$(CORE_SCHED_DIR)/include
CFLAGS += -I$(CORE_JOBS_DIR)/include
CFLAGS += -I$(CORE_WORKERS_DIR)/include
CFLAGS += -I$(CORE_WAKE_DIR)/include
CFLAGS += -I$(CORE_KERNEL_DIR)/include
CFLAGS += -I$(CORE_TRACE_DIR)/include
CFLAGS += -I$(CORE_THEME_DIR)/include
CFLAGS += -I$(CORE_FONT_DIR)/include

SRCS := $(shell find src -name '*.c')
OBJS := $(SRCS:src/%.c=build/%.o)
DEPS := $(OBJS:.o=.d)
LINK_OBJS := $(OBJS)
CORE_SHARED_LIBS := $(CORE_TRACE_LIB) $(CORE_PACK_LIB) $(CORE_KERNEL_LIB) $(CORE_WAKE_LIB) $(CORE_WORKERS_LIB) $(CORE_JOBS_LIB) $(CORE_SCHED_LIB) $(CORE_QUEUE_LIB) $(CORE_TIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_SPACE_LIB) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
LINK_OBJS += $(CORE_SHARED_LIBS)
TARGET := build/mapforge
TOOL_TARGET := build/tools/mapforge_region
TOOL_SRCS := tools/mapforge_region.c src/map/mercator.c src/map/tile_math.c src/core/log.c
GRAPH_TARGET := build/tools/mapforge_graph
GRAPH_SRCS := tools/mapforge_graph.c src/map/mercator.c src/core/log.c
MAP_SPACE_TEST_TARGET := build/tests/map_space_test
MAP_SPACE_TEST_SRCS := tests/map_space_test.c src/map/map_space.c src/map/tile_math.c src/map/mercator.c src/camera/camera.c
SHARED_THEME_FONT_ADAPTER_TEST_TARGET := build/tests/shared_theme_font_adapter_test
SHARED_THEME_FONT_ADAPTER_TEST_SRCS := tests/shared_theme_font_adapter_test.c src/ui/shared_theme_font_adapter.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c $(CORE_BASE_DIR)/src/core_base.c
MAP_TRACE_CONTRACT_TEST_TARGET := build/tests/map_trace_contract_test
MAP_TRACE_CONTRACT_TEST_SRCS := tests/map_trace_contract_test.c
APP_WORKER_CONTRACT_TEST_TARGET := build/tests/app_worker_contract_test
APP_WORKER_CONTRACT_TEST_SRCS := tests/app_worker_contract_test.c src/app/app_worker_contract.c
TILE_LOADER_SHUTDOWN_TEST_TARGET := build/tests/tile_loader_shutdown_test
TILE_LOADER_SHUTDOWN_TEST_SRCS := tests/tile_loader_shutdown_test.c src/map/tile_loader.c src/map/mft_loader.c src/map/polygon_cache.c src/map/polygon_triangulator.c src/core/log.c
APP_ROUTE_SERVICE_TEST_TARGET := build/tests/app_route_service_test
APP_ROUTE_SERVICE_TEST_SRCS := tests/app_route_service_test.c src/app/app_route_service.c

ifeq ($(VK_APP_ENABLED),1)
CFLAGS += -I$(VK_RENDERER_INCLUDE) -DMAPFORGE_HAVE_VK=1 -DVK_RENDERER_SHADER_ROOT=\"$(VK_RENDERER_RESOLVED_DIR)\"
LINK_OBJS += $(VK_RENDERER_OBJS)
LDLIBS += $(VULKAN_LIBS) -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
endif

MIN_Z ?= 10
MAX_Z ?= 18
RENDER_BACKEND ?= vulkan
VK_DEBUG ?= 0
OSM_DIR ?= $(HOME)/Desktop/osm_maps
REGIONS_DIR ?= data/regions
MAPFORGE_REGIONS_DIR ?= $(REGIONS_DIR)
BATCH_MODE ?= missing
BATCH_REGION ?=
BATCH_OSM ?=
BATCH_EXTRA_FLAGS ?=
PRUNE_DAYS ?= 30
KEEP_OLD ?= 1
REPLACE ?= 0
PRUNE_DRY_RUN ?= 0
PAD_BOUNDS ?= 0
EMIT_CONTOUR_EMPTY ?= 0
EMIT_LEGACY_TILES ?= 1

REGION_TOOL_FLAGS := $(if $(filter 1,$(REPLACE)),--replace,) \
	--keep-old $(KEEP_OLD) \
	--prune-days $(PRUNE_DAYS) \
	$(if $(filter 1,$(PRUNE_DRY_RUN)),--prune-dry-run,) \
	$(if $(filter 1,$(PAD_BOUNDS)),--pad-bounds,) \
	$(if $(filter 1,$(EMIT_CONTOUR_EMPTY)),--emit-contour-empty,) \
	$(if $(filter 1,$(EMIT_LEGACY_TILES)),--emit-legacy-tiles,--no-legacy-tiles)

GRAPH_TOOL_FLAGS := $(if $(filter 1,$(REPLACE)),--replace,) \
	--keep-old $(KEEP_OLD) \
	--prune-days $(PRUNE_DAYS) \
	$(if $(filter 1,$(PRUNE_DRY_RUN)),--prune-dry-run,)

app: $(TARGET)

$(CORE_BASE_LIB):
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_IO_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_IO_DIR)

$(CORE_DATA_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_DATA_DIR)

$(CORE_SPACE_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_SPACE_DIR)

$(CORE_PACK_LIB): $(CORE_IO_LIB)
	$(MAKE) -C $(CORE_PACK_DIR)

$(CORE_TIME_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_TIME_DIR)

$(CORE_QUEUE_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_QUEUE_DIR)

$(CORE_SCHED_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_SCHED_DIR)

$(CORE_JOBS_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_JOBS_DIR)

$(CORE_WORKERS_LIB): $(CORE_QUEUE_LIB)
	$(MAKE) -C $(CORE_WORKERS_DIR)

$(CORE_WAKE_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_WAKE_DIR)

$(CORE_KERNEL_LIB): $(CORE_SCHED_LIB) $(CORE_JOBS_LIB) $(CORE_WAKE_LIB) $(CORE_QUEUE_LIB) $(CORE_TIME_LIB)
	$(MAKE) -C $(CORE_KERNEL_DIR)

$(CORE_TRACE_LIB): $(CORE_PACK_LIB)
	$(MAKE) -C $(CORE_TRACE_DIR)

$(CORE_THEME_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_THEME_DIR)

$(CORE_FONT_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_FONT_DIR)

$(TARGET): $(LINK_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

build/vk_renderer/%.o: $(VK_RENDERER_RESOLVED_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

run: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) MAPFORGE_VK_DEBUG=$(VK_DEBUG) MAPFORGE_REGIONS_DIR="$(MAPFORGE_REGIONS_DIR)" ./$(TARGET)

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

tools: $(TOOL_TARGET)

$(TOOL_TARGET): $(TOOL_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(TOOL_SRCS) -o $@ $(TOOL_LDLIBS)

graph: $(GRAPH_TARGET)

$(GRAPH_TARGET): $(GRAPH_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(GRAPH_SRCS) -o $@ $(TOOL_LDLIBS)

test-space: $(MAP_SPACE_TEST_TARGET)
	./$(MAP_SPACE_TEST_TARGET)

build-safety-check: tools graph
	./tests/test_build_safety.sh

test: test-space build-safety-check
test: test-trace-contract
test: test-worker-contract
test: test-tile-loader-shutdown
test: test-route-service

test-shared-theme-font-adapter: $(SHARED_THEME_FONT_ADAPTER_TEST_TARGET)
	./$(SHARED_THEME_FONT_ADAPTER_TEST_TARGET)

test-trace-contract: $(MAP_TRACE_CONTRACT_TEST_TARGET)
	./$(MAP_TRACE_CONTRACT_TEST_TARGET)

test-worker-contract: $(APP_WORKER_CONTRACT_TEST_TARGET)
	./$(APP_WORKER_CONTRACT_TEST_TARGET)

test-tile-loader-shutdown: $(TILE_LOADER_SHUTDOWN_TEST_TARGET)
	./$(TILE_LOADER_SHUTDOWN_TEST_TARGET)

test-route-service: $(APP_ROUTE_SERVICE_TEST_TARGET)
	./$(APP_ROUTE_SERVICE_TEST_TARGET)

$(MAP_SPACE_TEST_TARGET): $(MAP_SPACE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(MAP_SPACE_TEST_SRCS) $(CORE_SPACE_LIB) $(CORE_BASE_LIB) -o $@ $(TOOL_LDLIBS)

$(SHARED_THEME_FONT_ADAPTER_TEST_TARGET): $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(MAP_TRACE_CONTRACT_TEST_TARGET): $(MAP_TRACE_CONTRACT_TEST_SRCS) $(CORE_TRACE_LIB) $(CORE_PACK_LIB) $(CORE_IO_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(MAP_TRACE_CONTRACT_TEST_SRCS) -o $@ $(CORE_TRACE_LIB) $(CORE_PACK_LIB) $(CORE_IO_LIB) $(CORE_BASE_LIB) -lm

$(APP_WORKER_CONTRACT_TEST_TARGET): $(APP_WORKER_CONTRACT_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(APP_WORKER_CONTRACT_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(TILE_LOADER_SHUTDOWN_TEST_TARGET): $(TILE_LOADER_SHUTDOWN_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(TILE_LOADER_SHUTDOWN_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_SHARED_LIBS)

$(APP_ROUTE_SERVICE_TEST_TARGET): $(APP_ROUTE_SERVICE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(APP_ROUTE_SERVICE_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

route: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" $(GRAPH_TOOL_FLAGS)

region: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

region-rebuild: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) --replace $(REGION_TOOL_FLAGS)

route-rebuild: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" --replace $(GRAPH_TOOL_FLAGS)

tools-progress:
	tools/run_with_progress.sh --label "make tools" make tools

graph-progress:
	tools/run_with_progress.sh --label "make graph" make graph

region-progress:
	tools/run_with_progress.sh --label "region $(REGION)" ./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

route-progress:
	tools/run_with_progress.sh --label "route $(REGION)" ./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" $(GRAPH_TOOL_FLAGS)

batch-regions:
	tools/build_regions.sh --osm-dir "$(OSM_DIR)" --regions-dir "$(REGIONS_DIR)" --min-z $(MIN_Z) --max-z $(MAX_Z) --keep-old $(KEEP_OLD) --prune-days $(PRUNE_DAYS) \
	$(if $(filter all,$(BATCH_MODE)),--all,--missing) \
	$(if $(BATCH_REGION),--region "$(BATCH_REGION)",) \
	$(if $(BATCH_OSM),--osm "$(BATCH_OSM)",) \
	$(BATCH_EXTRA_FLAGS)

disk-usage:
	@echo "=== MapForge Disk Usage ==="
	@du -sh build "$(REGIONS_DIR)" ide_files 2>/dev/null || true
	@echo ""
	@echo "=== Top Region Dirs ==="
	@if [ -d "$(REGIONS_DIR)" ] && [ -n "$$(ls -A "$(REGIONS_DIR)" 2>/dev/null)" ]; then \
		du -sh "$(REGIONS_DIR)"/* 2>/dev/null | sort -h; \
	else \
		echo "no regions present under $(REGIONS_DIR)"; \
	fi

region-clean:
	@if [ -z "$(REGION)" ]; then echo "REGION is required (example: make region-clean REGION=seattle)"; exit 1; fi
	@if ! printf "%s\n" "$(REGION)" | rg -q '^[A-Za-z0-9_.-]+$$'; then echo "invalid REGION name: $(REGION)"; exit 1; fi
	rm -rf "$(REGIONS_DIR)/$(REGION)"

graph-clean:
	@if [ -z "$(REGION)" ]; then echo "REGION is required (example: make graph-clean REGION=seattle)"; exit 1; fi
	@if ! printf "%s\n" "$(REGION)" | rg -q '^[A-Za-z0-9_.-]+$$'; then echo "invalid REGION name: $(REGION)"; exit 1; fi
	rm -rf "$(REGIONS_DIR)/$(REGION)/graph"

prune-regions:
	tools/prune_regions.sh --regions-dir "$(REGIONS_DIR)" --prune-days "$(PRUNE_DAYS)" --keep-old "$(KEEP_OLD)" $(if $(filter 1,$(PRUNE_DRY_RUN)),--dry-run,)

shared-check:
	@echo "=== Shared Library Check ==="
	@for path in "$(CORE_BASE_LIB)" "$(CORE_IO_LIB)" "$(CORE_DATA_LIB)" "$(CORE_SPACE_LIB)" "$(CORE_PACK_LIB)" "$(CORE_TIME_LIB)" "$(CORE_QUEUE_LIB)" "$(CORE_SCHED_LIB)" "$(CORE_JOBS_LIB)" "$(CORE_WORKERS_LIB)" "$(CORE_WAKE_LIB)" "$(CORE_KERNEL_LIB)" "$(CORE_TRACE_LIB)"; do \
		if [ ! -f "$$path" ]; then \
			echo "missing: $$path"; \
			exit 1; \
		fi; \
		echo "ok: $$path"; \
	done
	@echo ""
	@echo "=== Shared Versions ==="
	@for path in "$(CORE_BASE_DIR)/VERSION" "$(CORE_IO_DIR)/VERSION" "$(CORE_DATA_DIR)/VERSION" "$(CORE_SPACE_DIR)/VERSION" "$(CORE_PACK_DIR)/VERSION" "$(CORE_TIME_DIR)/VERSION" "$(CORE_QUEUE_DIR)/VERSION" "$(CORE_SCHED_DIR)/VERSION" "$(CORE_JOBS_DIR)/VERSION" "$(CORE_WORKERS_DIR)/VERSION" "$(CORE_WAKE_DIR)/VERSION" "$(CORE_KERNEL_DIR)/VERSION" "$(CORE_TRACE_DIR)/VERSION"; do \
		if [ -f "$$path" ]; then \
			printf "%s: " "$$path"; cat "$$path"; \
		else \
			echo "$$path: missing"; \
			exit 1; \
		fi; \
	done

trace-latest:
	@latest=$$(ls -1t build/traces/*.pack 2>/dev/null | head -n 1); \
	if [ -z "$$latest" ]; then \
		echo "no trace packs found under build/traces"; \
		exit 1; \
	fi; \
	echo "inspecting $$latest"; \
	$(MAKE) -C $(CORE_PACK_DIR) tools >/dev/null; \
	$(CORE_PACK_DIR)/build/pack_cli inspect "$$latest"

vk-lib:
	@if [ ! -f "$(VK_RENDERER_RESOLVED_DIR)/include/vk_renderer.h" ]; then \
		echo "vk renderer not found at $(VK_RENDERER_DIR)"; \
		exit 1; \
	fi
	$(MAKE) -C "$(VK_RENDERER_RESOLVED_DIR)" all
	@mkdir -p $(dir $(VK_BUILD_LIB)) $(VK_BUILD_SHADER_DIR)
	@cp "$(VK_RENDERER_RESOLVED_DIR)/build/lib/libvkrenderer.a" "$(VK_BUILD_LIB)"
	@for shader in $(VK_REQUIRED_SHADERS); do \
		cp "$(VK_RENDERER_RESOLVED_DIR)/shaders/$$shader" "$(VK_BUILD_SHADER_DIR)/$$shader"; \
	done

vk-check: vk-lib
	@echo "vk renderer dir: $(VK_RENDERER_RESOLVED_DIR)"
	@echo "checking required vk symbols..."
	@nm -g "$(VK_BUILD_LIB)" | rg -q "vk_renderer_init" || (echo "missing symbol: vk_renderer_init" && exit 1)
	@nm -g "$(VK_BUILD_LIB)" | rg -q "vk_renderer_begin_frame" || (echo "missing symbol: vk_renderer_begin_frame" && exit 1)
	@nm -g "$(VK_BUILD_LIB)" | rg -q "vk_renderer_end_frame" || (echo "missing symbol: vk_renderer_end_frame" && exit 1)
	@echo "checking required shaders..."
	@for shader in $(VK_REQUIRED_SHADERS); do \
		test -f "$(VK_BUILD_SHADER_DIR)/$$shader" || (echo "missing shader: $$shader" && exit 1); \
	done
	@echo "vulkan checks passed."

clean:
	rm -rf build

.PHONY: app run run-ide-theme run-daw-theme tools graph test-space build-safety-check test test-shared-theme-font-adapter test-trace-contract test-worker-contract test-tile-loader-shutdown test-route-service route route-rebuild region region-rebuild tools-progress graph-progress region-progress route-progress batch-regions disk-usage region-clean graph-clean prune-regions shared-check trace-latest vk-lib vk-check clean

-include $(DEPS)
