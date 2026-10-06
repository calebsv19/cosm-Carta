CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
PKG_CONFIG ?= pkg-config

CORE_BASE_DIR ?= ../../core/core_base
CORE_THEME_DIR ?= ../../core/core_theme
CORE_FONT_DIR ?= ../../core/core_font
KIT_RENDER_DIR ?= ../kit_render
KIT_RENDER_ENABLE_VK ?= 0
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/default
ifeq ($(KIT_RENDER_ENABLE_VK),1)
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/vk
endif
VK_RENDERER_DIR ?= ../../vk_renderer
VK_RUNTIME_DIR ?= ../../vk_runtime
INC = -Iinclude -I$(KIT_RENDER_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include
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
INC += -I$(VK_RENDERER_DIR)/include -I$(VK_RUNTIME_DIR)/include $(VULKAN_CFLAGS) $(SDL_CFLAGS) $(SDL_TTF_CFLAGS)
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

LIB = $(OBJ_DIR)/libkit_ui.a
LIB_OBJ = $(OBJ_DIR)/kit_ui.o $(OBJ_DIR)/kit_ui_button.o $(OBJ_DIR)/kit_ui_hud.o $(OBJ_DIR)/kit_ui_interaction.o $(OBJ_DIR)/kit_ui_surface.o $(OBJ_DIR)/kit_ui_text_edit.o $(OBJ_DIR)/kit_ui_focus_scope.o $(OBJ_DIR)/kit_ui_text_presentation.o $(OBJ_DIR)/kit_ui_text_presentation_render.o $(OBJ_DIR)/kit_ui_focus_order.o
TEST_BIN = $(OBJ_DIR)/kit_ui_test
VALIDATION_BIN = $(OBJ_DIR)/kit_ui_vk_validation

.PHONY: all clean test validation-harness vk-renderer-lib

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(KIT_RENDER_BUILD_DIR)/libkit_render.a:
	$(MAKE) -C $(KIT_RENDER_DIR) KIT_RENDER_ENABLE_VK=$(KIT_RENDER_ENABLE_VK)

../../core/core_base/build/libcore_base.a:
	$(MAKE) -C ../../core/core_base

../../core/core_theme/build/libcore_theme.a: ../../core/core_base/build/libcore_base.a
	$(MAKE) -C ../../core/core_theme

../../core/core_font/build/libcore_font.a: ../../core/core_base/build/libcore_base.a
	$(MAKE) -C ../../core/core_font

vk-renderer-lib:
	$(MAKE) -C $(VK_RENDERER_DIR)

$(OBJ_DIR)/kit_ui.o: src/kit_ui.c include/kit_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_ui_button.o: src/kit_ui_button.c include/kit_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_ui_hud.o: src/kit_ui_hud.c include/kit_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_ui_interaction.o: src/kit_ui_interaction.c include/kit_ui_interaction.h include/kit_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(OBJ_DIR)/kit_ui_surface.o: src/kit_ui_surface.c include/kit_ui_surface.h include/kit_ui_interaction.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(TEST_BIN): tests/kit_ui_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a -o $@

test: $(TEST_BIN) test-interaction test-surface test-text-edit test-text-presentation
	./$(TEST_BIN)

# SDL stays optional: the generic archive/test do not link SDL or this adapter.
.PHONY: test-sdl-appearance
test-sdl-appearance: $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) $(shell $(PKG_CONFIG) --cflags sdl2) tests/kit_ui_sdl_appearance_test.c src/kit_ui_sdl.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a $(shell $(PKG_CONFIG) --libs sdl2) -lm -o $(OBJ_DIR)/kit_ui_sdl_appearance_test
	./$(OBJ_DIR)/kit_ui_sdl_appearance_test

$(VALIDATION_BIN): examples/kit_ui_vk_validation.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a vk-renderer-lib ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a | $(OBJ_DIR)
ifeq ($(KIT_RENDER_ENABLE_VK),1)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(VK_RENDERER_DIR)/build/lib/libvkrenderer.a $(VK_RUNTIME_DIR)/build/lib/libvkruntime.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a $(VULKAN_LIBS) $(SDL_LIBS) $(SDL_TTF_LIBS) $(APPLE_FW) -o $@
else
	@echo "validation-harness requires KIT_RENDER_ENABLE_VK=1"
	@false
endif

validation-harness: $(VALIDATION_BIN)
	@echo "Built $(VALIDATION_BIN)"

# Bounded real-pixel proof; unlike validation-harness, this executes and checks captures.
.PHONY: test-rounded-vk
test-rounded-vk: $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a vk-renderer-lib ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a
	@test "$(KIT_RENDER_ENABLE_VK)" = 1 || { echo "test-rounded-vk requires KIT_RENDER_ENABLE_VK=1"; exit 1; }
	$(CC) $(CFLAGS) $(INC) tests/kit_ui_rounded_vk_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(VK_RENDERER_DIR)/build/lib/libvkrenderer.a $(VK_RUNTIME_DIR)/build/lib/libvkruntime.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a $(VULKAN_LIBS) $(SDL_LIBS) $(SDL_TTF_LIBS) $(APPLE_FW) -lm -o $(OBJ_DIR)/kit_ui_rounded_vk_test
	$(if $(filter Darwin,$(shell uname -s)),DYLD_LIBRARY_PATH=/opt/homebrew/lib,) ./$(OBJ_DIR)/kit_ui_rounded_vk_test

clean:
	rm -rf $(OBJ_DIR)

.PHONY: test-surface
test-surface: $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) tests/kit_ui_surface_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a -lm -o $(OBJ_DIR)/kit_ui_surface_test
	./$(OBJ_DIR)/kit_ui_surface_test

.PHONY: test-interaction test-interaction-sdl
test-interaction: $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) tests/kit_ui_interaction_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a -lm -o $(OBJ_DIR)/kit_ui_interaction_test
	./$(OBJ_DIR)/kit_ui_interaction_test

test-interaction-sdl: $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) $(shell $(PKG_CONFIG) --cflags sdl2) tests/kit_ui_interaction_sdl_test.c src/kit_ui_interaction_sdl.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a $(shell $(PKG_CONFIG) --libs sdl2) -lm -o $(OBJ_DIR)/kit_ui_interaction_sdl_test
	./$(OBJ_DIR)/kit_ui_interaction_sdl_test

$(OBJ_DIR)/kit_ui_text_edit.o: src/kit_ui_text_edit.c include/kit_ui_text_edit.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
$(OBJ_DIR)/kit_ui_focus_scope.o: src/kit_ui_focus_scope.c include/kit_ui_focus_scope.h include/kit_ui_surface.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
.PHONY: test-text-edit test-text-edit-sdl
test-text-edit: $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) tests/kit_ui_text_edit_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a -lm -o $(OBJ_DIR)/kit_ui_text_edit_test
	./$(OBJ_DIR)/kit_ui_text_edit_test
test-text-edit-sdl: $(LIB)
	$(CC) $(CFLAGS) $(INC) $(shell $(PKG_CONFIG) --cflags sdl2) tests/kit_ui_text_edit_sdl_test.c src/kit_ui_text_edit_sdl.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a $(shell $(PKG_CONFIG) --libs sdl2) -lm -o $(OBJ_DIR)/kit_ui_text_edit_sdl_test
	./$(OBJ_DIR)/kit_ui_text_edit_sdl_test

$(OBJ_DIR)/kit_ui_text_presentation.o: src/kit_ui_text_presentation.c include/kit_ui_text_presentation.h include/kit_ui_text_edit.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
$(OBJ_DIR)/kit_ui_text_presentation_render.o: src/kit_ui_text_presentation_render.c include/kit_ui_text_presentation.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

.PHONY: test-text-presentation
test-text-presentation: $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) tests/kit_ui_text_presentation_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a ../../core/core_theme/build/libcore_theme.a ../../core/core_font/build/libcore_font.a ../../core/core_base/build/libcore_base.a -lm -o $(OBJ_DIR)/kit_ui_text_presentation_test
	./$(OBJ_DIR)/kit_ui_text_presentation_test

$(OBJ_DIR)/kit_ui_focus_order.o: src/kit_ui_focus_order.c include/kit_ui_focus_order.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

.PHONY: test-focus-order test-native-text-sdl
test: test-focus-order
test-focus-order: $(LIB) ../../core/core_base/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) tests/kit_ui_focus_order_test.c $(LIB) ../../core/core_base/build/libcore_base.a -o $(OBJ_DIR)/kit_ui_focus_order_test
	./$(OBJ_DIR)/kit_ui_focus_order_test
test-native-text-sdl: $(LIB)
	$(CC) $(CFLAGS) $(INC) $(shell $(PKG_CONFIG) --cflags sdl2) tests/kit_ui_native_text_sdl_test.c src/kit_ui_native_text_sdl.c $(LIB) ../../core/core_base/build/libcore_base.a $(shell $(PKG_CONFIG) --libs sdl2) -lm -o $(OBJ_DIR)/kit_ui_native_text_sdl_test
	./$(OBJ_DIR)/kit_ui_native_text_sdl_test

.PHONY: test-window-sdl
test-window-sdl:
	@mkdir -p build
	$(CC) $(CFLAGS) $(INC) $(shell $(PKG_CONFIG) --cflags sdl2) tests/kit_ui_window_sdl_test.c src/kit_ui_window_sdl.c $(shell $(PKG_CONFIG) --libs sdl2) -lm -o build/kit_ui_window_sdl_test
	SDL_VIDEODRIVER=dummy ./build/kit_ui_window_sdl_test
