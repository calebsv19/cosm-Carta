CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
PKG_CONFIG ?= pkg-config

CORE_BASE_DIR ?= ../../core/core_base
CORE_THEME_DIR ?= ../../core/core_theme
CORE_FONT_DIR ?= ../../core/core_font
KIT_RENDER_DIR ?= ../../kit/kit_render
KIT_UI_DIR ?= ../../kit/kit_ui
KIT_GRAPH_TS_DIR ?= ../../kit/kit_graph_timeseries
KIT_GRAPH_STRUCT_DIR ?= ../../kit/kit_graph_struct
VK_RENDERER_DIR ?= ../../vk_renderer
KIT_RENDER_ENABLE_VK ?= 1

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

INC = -Isrc -I$(KIT_RENDER_DIR)/include -I$(KIT_UI_DIR)/include -I$(KIT_GRAPH_TS_DIR)/include -I$(KIT_GRAPH_STRUCT_DIR)/include -I$(VK_RENDERER_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include $(VULKAN_CFLAGS) $(SDL_CFLAGS) $(SDL_TTF_CFLAGS)

OBJ_DIR = build
BIN = $(OBJ_DIR)/kit_visual_showcase

.PHONY: all clean run vk-renderer-lib

all: $(BIN)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(KIT_RENDER_DIR)/build/vk/libkit_render.a:
	$(MAKE) -C $(KIT_RENDER_DIR) KIT_RENDER_ENABLE_VK=1

$(KIT_UI_DIR)/build/libkit_ui.a: $(KIT_RENDER_DIR)/build/vk/libkit_render.a
	$(MAKE) -C $(KIT_UI_DIR) KIT_RENDER_ENABLE_VK=1

$(KIT_GRAPH_TS_DIR)/build/libkit_graph_timeseries.a: $(KIT_UI_DIR)/build/libkit_ui.a
	$(MAKE) -C $(KIT_GRAPH_TS_DIR) KIT_RENDER_ENABLE_VK=1

$(KIT_GRAPH_STRUCT_DIR)/build/libkit_graph_struct.a: $(KIT_UI_DIR)/build/libkit_ui.a
	$(MAKE) -C $(KIT_GRAPH_STRUCT_DIR) KIT_RENDER_ENABLE_VK=1

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_THEME_DIR)/build/libcore_theme.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_THEME_DIR)

$(CORE_FONT_DIR)/build/libcore_font.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_FONT_DIR)

vk-renderer-lib:
	$(MAKE) -C $(VK_RENDERER_DIR)

$(BIN): src/kit_visual_showcase.c $(KIT_GRAPH_STRUCT_DIR)/build/libkit_graph_struct.a $(KIT_GRAPH_TS_DIR)/build/libkit_graph_timeseries.a $(KIT_UI_DIR)/build/libkit_ui.a $(KIT_RENDER_DIR)/build/vk/libkit_render.a vk-renderer-lib $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(KIT_GRAPH_STRUCT_DIR)/build/libkit_graph_struct.a $(KIT_GRAPH_TS_DIR)/build/libkit_graph_timeseries.a $(KIT_UI_DIR)/build/libkit_ui.a $(KIT_RENDER_DIR)/build/vk/libkit_render.a $(VK_RENDERER_DIR)/build/lib/libvkrenderer.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a $(VULKAN_LIBS) $(SDL_LIBS) $(SDL_TTF_LIBS) $(APPLE_FW) -lm -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(OBJ_DIR)
