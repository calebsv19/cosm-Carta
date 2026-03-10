CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
PKG_CONFIG ?= pkg-config

CORE_BASE_DIR ?= ../../core/core_base
CORE_THEME_DIR ?= ../../core/core_theme
CORE_FONT_DIR ?= ../../core/core_font
VK_RENDERER_DIR ?= ../../vk_renderer
KIT_RENDER_ENABLE_VK ?= 0

INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include
VK_ENABLE_CFLAGS = -DKIT_RENDER_ENABLE_VK_BACKEND=$(KIT_RENDER_ENABLE_VK)

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
endif
OBJ_DIR = build/default
ifeq ($(KIT_RENDER_ENABLE_VK),1)
OBJ_DIR = build/vk
endif

LIB = $(OBJ_DIR)/libkit_render.a
LIB_OBJ = $(OBJ_DIR)/kit_render.o $(OBJ_DIR)/kit_render_backend_null.o $(OBJ_DIR)/kit_render_backend_vk.o
TEST_BIN = $(OBJ_DIR)/kit_render_test
VALIDATION_BIN = $(OBJ_DIR)/kit_render_vk_validation

VULKAN_LIBS :=
SDL_LIBS :=
APPLE_FW :=
ifeq ($(KIT_RENDER_ENABLE_VK),1)
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

.PHONY: all clean test validation-harness vk-renderer-lib

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_THEME_DIR)/build/libcore_theme.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_THEME_DIR)

$(CORE_FONT_DIR)/build/libcore_font.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_FONT_DIR)

vk-renderer-lib:
	$(MAKE) -C $(VK_RENDERER_DIR)

$(OBJ_DIR)/kit_render.o: src/kit_render.c src/kit_render_backend.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(VK_ENABLE_CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_render_backend_null.o: src/kit_render_backend_null.c src/kit_render_backend.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(VK_ENABLE_CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_render_backend_vk.o: src/kit_render_backend_vk.c src/kit_render_backend.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(VK_ENABLE_CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/kit_render_test.c $(LIB) $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(VALIDATION_BIN): examples/kit_render_vk_validation.c $(LIB) vk-renderer-lib $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
ifeq ($(KIT_RENDER_ENABLE_VK),1)
	$(CC) $(CFLAGS) $(VK_ENABLE_CFLAGS) $(INC) $< $(LIB) $(VK_RENDERER_DIR)/build/lib/libvkrenderer.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a $(VULKAN_LIBS) $(SDL_LIBS) $(SDL_TTF_LIBS) $(APPLE_FW) -o $@
else
	@echo "validation-harness requires KIT_RENDER_ENABLE_VK=1"
	@false
endif

validation-harness: $(VALIDATION_BIN)
	@echo "Built $(VALIDATION_BIN)"

clean:
	rm -rf $(OBJ_DIR)
