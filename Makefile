CC := cc

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SDL_TTF_CFLAGS := $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell pkg-config --libs SDL2_ttf 2>/dev/null)
VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)

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
LDLIBS := $(SDL_LIBS) $(SDL_TTF_LIBS) -pthread
TOOL_LDLIBS := -lm

SRCS := $(shell find src -name '*.c')
OBJS := $(SRCS:src/%.c=build/%.o)
DEPS := $(OBJS:.o=.d)
LINK_OBJS := $(OBJS)
TARGET := build/mapforge
TOOL_TARGET := build/tools/mapforge_region
TOOL_SRCS := tools/mapforge_region.c src/map/mercator.c src/map/tile_math.c src/core/log.c
GRAPH_TARGET := build/tools/mapforge_graph
GRAPH_SRCS := tools/mapforge_graph.c src/map/mercator.c src/core/log.c

ifeq ($(VK_APP_ENABLED),1)
CFLAGS += -I$(VK_RENDERER_INCLUDE) -DMAPFORGE_HAVE_VK=1 -DVK_RENDERER_SHADER_ROOT=\"$(VK_RENDERER_RESOLVED_DIR)\"
LINK_OBJS += $(VK_RENDERER_OBJS)
LDLIBS += $(VULKAN_LIBS) -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
endif

MIN_Z ?= 12
MAX_Z ?= 12
RENDER_BACKEND ?= vulkan

app: $(TARGET)

$(TARGET): $(LINK_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

build/vk_renderer/%.o: $(VK_RENDERER_RESOLVED_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

run: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) ./$(TARGET)

tools: $(TOOL_TARGET)

$(TOOL_TARGET): $(TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(TOOL_SRCS) -o $@ $(TOOL_LDLIBS)

graph: $(GRAPH_TARGET)

$(GRAPH_TARGET): $(GRAPH_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(GRAPH_SRCS) -o $@ $(TOOL_LDLIBS)

route: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out data/regions/$(REGION)

region: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out data/regions/$(REGION) --min-z $(MIN_Z) --max-z $(MAX_Z)

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

.PHONY: app run tools graph route region vk-lib vk-check clean

-include $(DEPS)
