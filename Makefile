CC := cc

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SDL_TTF_CFLAGS := $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell pkg-config --libs SDL2_ttf 2>/dev/null)
VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
JSON_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)
CORE_SPACE_DIR := ../shared/core_space
CORE_BASE_DIR := ../shared/core_base
CORE_IO_DIR := ../shared/core_io

VK_RENDERER_DIR ?= third_party/vk_renderer
VK_RENDERER_FALLBACK_DIR ?= $(HOME)/Desktop/CodeWork/shared/vk_renderer
VK_RENDERER_RESOLVED_DIR := $(if $(wildcard $(VK_RENDERER_DIR)/include/vk_renderer.h),$(VK_RENDERER_DIR),$(VK_RENDERER_FALLBACK_DIR))
VK_RENDERER_INCLUDE := $(VK_RENDERER_RESOLVED_DIR)/include
VK_RENDERER_STATIC_LIB := $(VK_RENDERER_RESOLVED_DIR)/build/lib/libvkrenderer.a
VK_RENDERER_SRCS := $(wildcard $(VK_RENDERER_RESOLVED_DIR)/src/*.c)
VK_RENDERER_OBJS := $(patsubst $(VK_RENDERER_RESOLVED_DIR)/src/%.c,build/vk_renderer/%.o,$(VK_RENDERER_SRCS))
VK_BUILD_LIB := build/vk/lib/libvkrenderer.a
VK_BUILD_SHADER_DIR := build/vk/shaders
VK_REQUIRED_SHADERS := fill.vert.spv fill.frag.spv line.vert.spv line.frag.spv textured.vert.spv textured.frag.spv
VK_APP_ENABLED := $(if $(wildcard $(VK_RENDERER_INCLUDE)/vk_renderer.h),$(if $(wildcard $(VK_RENDERER_STATIC_LIB)),1,))

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
TOOL_LDLIBS := -lm

ifeq ($(JSON_LIBS),)
LDLIBS += -ljson-c
endif
CFLAGS += $(JSON_CFLAGS)
CFLAGS += -I$(CORE_SPACE_DIR)/include
CFLAGS += -I$(CORE_BASE_DIR)/include
CFLAGS += -I$(CORE_IO_DIR)/include

SRCS := $(shell find src -name '*.c')
OBJS := $(SRCS:src/%.c=build/%.o)
DEPS := $(OBJS:.o=.d)
LINK_OBJS := $(OBJS)
CORE_SPACE_SRCS := $(CORE_SPACE_DIR)/src/core_space.c
CORE_SPACE_OBJS := $(patsubst $(CORE_SPACE_DIR)/src/%.c,build/core_space/%.o,$(CORE_SPACE_SRCS))
CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,build/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,build/core_io/%.o,$(CORE_IO_SRCS))
LINK_OBJS += $(CORE_SPACE_OBJS)
LINK_OBJS += $(CORE_BASE_OBJS)
LINK_OBJS += $(CORE_IO_OBJS)
TARGET := build/mapforge
TOOL_TARGET := build/tools/mapforge_region
TOOL_SRCS := tools/mapforge_region.c src/map/mercator.c src/map/tile_math.c src/core/log.c
GRAPH_TARGET := build/tools/mapforge_graph
GRAPH_SRCS := tools/mapforge_graph.c src/map/mercator.c src/core/log.c
MAP_SPACE_TEST_TARGET := build/tests/map_space_test
MAP_SPACE_TEST_SRCS := tests/map_space_test.c src/map/map_space.c src/map/tile_math.c src/map/mercator.c src/camera/camera.c $(CORE_SPACE_DIR)/src/core_space.c

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

$(TARGET): $(LINK_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

build/vk_renderer/%.o: $(VK_RENDERER_RESOLVED_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

build/core_space/%.o: $(CORE_SPACE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

build/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

build/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

run: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) MAPFORGE_VK_DEBUG=$(VK_DEBUG) ./$(TARGET)

tools: $(TOOL_TARGET)

$(TOOL_TARGET): $(TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(TOOL_SRCS) -o $@ $(TOOL_LDLIBS)

graph: $(GRAPH_TARGET)

$(GRAPH_TARGET): $(GRAPH_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(GRAPH_SRCS) -o $@ $(TOOL_LDLIBS)

test-space: $(MAP_SPACE_TEST_TARGET)
	./$(MAP_SPACE_TEST_TARGET)

build-safety-check: tools graph
	./tests/test_build_safety.sh

test: test-space build-safety-check

$(MAP_SPACE_TEST_TARGET): $(MAP_SPACE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(MAP_SPACE_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

route: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out data/regions/$(REGION) $(GRAPH_TOOL_FLAGS)

region: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out data/regions/$(REGION) --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

region-rebuild: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out data/regions/$(REGION) --min-z $(MIN_Z) --max-z $(MAX_Z) --replace $(REGION_TOOL_FLAGS)

route-rebuild: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out data/regions/$(REGION) --replace $(GRAPH_TOOL_FLAGS)

tools-progress:
	tools/run_with_progress.sh --label "make tools" make tools

graph-progress:
	tools/run_with_progress.sh --label "make graph" make graph

region-progress:
	tools/run_with_progress.sh --label "region $(REGION)" ./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out data/regions/$(REGION) --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

route-progress:
	tools/run_with_progress.sh --label "route $(REGION)" ./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out data/regions/$(REGION) $(GRAPH_TOOL_FLAGS)

batch-regions:
	tools/build_regions.sh --osm-dir "$(OSM_DIR)" --regions-dir "$(REGIONS_DIR)" --min-z $(MIN_Z) --max-z $(MAX_Z) --keep-old $(KEEP_OLD) --prune-days $(PRUNE_DAYS) \
	$(if $(filter all,$(BATCH_MODE)),--all,--missing) \
	$(if $(BATCH_REGION),--region "$(BATCH_REGION)",) \
	$(if $(BATCH_OSM),--osm "$(BATCH_OSM)",) \
	$(BATCH_EXTRA_FLAGS)

disk-usage:
	@echo "=== MapForge Disk Usage ==="
	@du -sh build data/regions ide_files 2>/dev/null || true
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

vk-lib:
	@if [ ! -f "$(VK_RENDERER_RESOLVED_DIR)/include/vk_renderer.h" ]; then \
		echo "vk renderer not found at $(VK_RENDERER_DIR) or $(VK_RENDERER_FALLBACK_DIR)"; \
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

.PHONY: app run tools graph test-space build-safety-check test route route-rebuild region region-rebuild tools-progress graph-progress region-progress route-progress batch-regions disk-usage region-clean graph-clean prune-regions vk-lib vk-check clean

-include $(DEPS)
