CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
PKG_CONFIG ?= pkg-config

CORE_BASE_DIR ?= ../../core/core_base
CORE_THEME_DIR ?= ../../core/core_theme
CORE_FONT_DIR ?= ../../core/core_font
KIT_RENDER_DIR ?= ../kit_render
KIT_UI_DIR ?= ../kit_ui
KIT_RENDER_ENABLE_VK ?= 0
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/default
ifeq ($(KIT_RENDER_ENABLE_VK),1)
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/vk
endif
VK_RENDERER_DIR ?= ../../vk_renderer

INC = -Iinclude -I$(KIT_RENDER_DIR)/include -I$(KIT_UI_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include
ifeq ($(KIT_RENDER_ENABLE_VK),1)
VULKAN_CFLAGS := $(shell $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_TTF_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2_ttf 2>/dev/null)
ifeq ($(strip $(VULKAN_CFLAGS)),)
  VULKAN_CFLAGS := -I/opt/homebrew/include
endif
ifeq ($(strip $(SDL_CFLAGS)),)
  SDL_CFLAGS := -I/opt/homebrew/include/SDL2
endif
ifeq ($(strip $(SDL_TTF_CFLAGS)),)
  SDL_TTF_CFLAGS := -I/opt/homebrew/include/SDL2
endif
INC += -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) $(SDL_CFLAGS) $(SDL_TTF_CFLAGS)
VULKAN_LIBS := $(shell $(PKG_CONFIG) --libs vulkan 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)
SDL_TTF_LIBS := $(shell $(PKG_CONFIG) --libs sdl2_ttf 2>/dev/null)
ifeq ($(strip $(VULKAN_LIBS)),)
  VULKAN_LIBS := -L/opt/homebrew/lib -lvulkan
endif
ifeq ($(strip $(SDL_LIBS)),)
  SDL_LIBS := -L/opt/homebrew/lib -lSDL2
endif
ifeq ($(strip $(SDL_TTF_LIBS)),)
  SDL_TTF_LIBS := -L/opt/homebrew/lib -lSDL2_ttf
endif
APPLE_FW := -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
endif

OBJ_DIR = build
LIB = $(OBJ_DIR)/libkit_graph_timeseries.a
LIB_OBJ = $(OBJ_DIR)/kit_graph_timeseries_math.o $(OBJ_DIR)/kit_graph_timeseries_draw.o
TEST_BIN = $(OBJ_DIR)/kit_graph_timeseries_test
VALIDATION_BIN = $(OBJ_DIR)/kit_graph_timeseries_vk_validation

.PHONY: all clean test validation-harness vk-renderer-lib

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(KIT_RENDER_BUILD_DIR)/libkit_render.a:
	$(MAKE) -C $(KIT_RENDER_DIR) KIT_RENDER_ENABLE_VK=$(KIT_RENDER_ENABLE_VK)

$(KIT_UI_DIR)/build/libkit_ui.a: $(KIT_RENDER_BUILD_DIR)/libkit_render.a
	$(MAKE) -C $(KIT_UI_DIR) KIT_RENDER_ENABLE_VK=$(KIT_RENDER_ENABLE_VK)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_THEME_DIR)/build/libcore_theme.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_THEME_DIR)

$(CORE_FONT_DIR)/build/libcore_font.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_FONT_DIR)

vk-renderer-lib:
	$(MAKE) -C $(VK_RENDERER_DIR)

$(OBJ_DIR)/kit_graph_timeseries_math.o: src/kit_graph_timeseries_math.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_graph_timeseries_draw.o: src/kit_graph_timeseries_draw.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	rm -f $@
	ar rcs $@ $^

$(TEST_BIN): tests/kit_graph_timeseries_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(VALIDATION_BIN): examples/kit_graph_timeseries_vk_validation.c $(LIB) $(KIT_UI_DIR)/build/libkit_ui.a $(KIT_RENDER_BUILD_DIR)/libkit_render.a vk-renderer-lib $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
ifeq ($(KIT_RENDER_ENABLE_VK),1)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(KIT_UI_DIR)/build/libkit_ui.a $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(VK_RENDERER_DIR)/build/lib/libvkrenderer.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a $(VULKAN_LIBS) $(SDL_LIBS) $(SDL_TTF_LIBS) $(APPLE_FW) -lm -o $@
else
	@echo "validation-harness requires KIT_RENDER_ENABLE_VK=1"
	@false
endif

validation-harness: $(VALIDATION_BIN)
	@echo "Built $(VALIDATION_BIN)"

clean:
	rm -rf $(OBJ_DIR)
