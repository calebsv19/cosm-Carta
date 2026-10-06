# ===== Makefile: Vulkan + SDL2 (MoltenVK on macOS) =====

# Tools
CC         ?= cc
AR         ?= ar
PKG_CONFIG ?= pkg-config

# Flags
CSTD   ?= -std=c11
CWARN  ?= -Wall -Wextra -Wpedantic
COPT   ?= -O2

# Project includes
VK_RUNTIME_ROOT ?= ../vk_runtime
INC := -Iinclude -I$(VK_RUNTIME_ROOT)/include

# --- Vulkan flags (prefer pkg-config) ---
VULKAN_CFLAGS := $(shell $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
VULKAN_LIBS   := $(shell $(PKG_CONFIG) --libs   vulkan 2>/dev/null)
ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
  VULKAN_CFLAGS := -I/opt/homebrew/include
  VULKAN_LIBS   := -L/opt/homebrew/lib -lvulkan
endif

# --- SDL2 flags (prefer pkg-config) ---
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs   sdl2 2>/dev/null)
ifeq ($(strip $(SDL_CFLAGS)$(SDL_LIBS)),)
  SDL_CFLAGS := -I/opt/homebrew/include/SDL2
  SDL_LIBS   := -L/opt/homebrew/lib -lSDL2
endif

# macOS frameworks (Metal for MoltenVK, Cocoa boilerplate for SDL)
UNAME_S := $(shell uname -s)
APPLE_FW :=
PLATFORM_CFLAGS :=
VALIDATION_LAYER_DIR ?= $(patsubst %/,%,$(dir $(firstword \
	$(wildcard /opt/homebrew/lib/libVkLayer_khronos_validation.dylib) \
	$(wildcard /usr/local/lib/libVkLayer_khronos_validation.dylib))))
ifeq ($(UNAME_S),Darwin)
  APPLE_FW := -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
  PLATFORM_CFLAGS := -DVK_USE_PLATFORM_METAL_EXT
  VALIDATION_ENV := $(if $(VALIDATION_LAYER_DIR),DYLD_LIBRARY_PATH=$(VALIDATION_LAYER_DIR),)
else
  VALIDATION_ENV :=
endif

# Common compile/link flags
CFLAGS  := $(COPT) $(CSTD) $(CWARN) $(INC) $(VULKAN_CFLAGS) $(SDL_CFLAGS) -D_THREAD_SAFE $(PLATFORM_CFLAGS) -DVK_RENDERER_SHADER_ROOT=\"$(CURDIR)\"
LDFLAGS := -L$(VK_RUNTIME_ROOT)/build/lib -lvkruntime $(VULKAN_LIBS) $(SDL_LIBS) $(APPLE_FW) -lm

# Sources / objects
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/obj/%.o,$(SRC))

# Outputs
LIBDIR := build/lib
BINDIR := build/example
TESTDIR := build/tests
LIB    := $(LIBDIR)/libvkrenderer.a

# Default target
.PHONY: all
all: runtime $(LIB)

.PHONY: runtime
runtime:
	$(MAKE) -C $(VK_RUNTIME_ROOT) all

# Build static library from all src/*.c
$(LIB): $(OBJ)
	@mkdir -p $(LIBDIR)
	$(AR) rcs $@ $(OBJ)

# Compile .c -> .o
build/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

# Example (optional)
.PHONY: example
example: runtime $(LIB)
	@test -f examples/sdl_vulkan_example.c || { echo "No examples/sdl_vulkan_example.c present; skipping."; exit 0; }
	@mkdir -p $(BINDIR)
	$(CC) examples/sdl_vulkan_example.c $(CFLAGS) -L$(LIBDIR) -lvkrenderer -o $(BINDIR)/sdl_vulkan_example $(LDFLAGS)
	@echo "Built $(BINDIR)/sdl_vulkan_example"

.PHONY: test-live
test-live: runtime $(LIB)
	@mkdir -p $(TESTDIR)
	$(CC) tests/test_vk_renderer_live.c $(CFLAGS) -L$(LIBDIR) -lvkrenderer -o $(TESTDIR)/test_vk_renderer_live $(LDFLAGS)
	$(VALIDATION_ENV) ./$(TESTDIR)/test_vk_renderer_live

# Shader helper passthrough (your script already exists)
.PHONY: shaders
shaders:
	./scripts/compile_shaders.sh

# Debug flags
.PHONY: print-flags
print-flags:
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "SRC = $(SRC)"

.PHONY: clean
clean:
	rm -rf build

.PHONY: test-texture-corners
test-texture-corners:
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) tests/test_texture_corners.c src/vk_renderer_texture_corners.c -o $(TESTDIR)/test_texture_corners $(SDL_LIBS) -lm
	./$(TESTDIR)/test_texture_corners

.PHONY: test-frame-submission
test-frame-submission:
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) tests/test_frame_submission.c src/vk_renderer_frame_submission.c -o $(TESTDIR)/test_frame_submission
	./$(TESTDIR)/test_frame_submission

.PHONY: test-window-live
test-window-live: runtime $(LIB)
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -I../kit/kit_ui/include -I../core/core_base/include tests/test_window_lifecycle.c ../kit/kit_ui/src/kit_ui_window_sdl.c ../kit/kit_ui/src/kit_ui_window_probe_sdl.c -L$(LIBDIR) -lvkrenderer $(LDFLAGS) -o $(TESTDIR)/test_window_lifecycle
	$(VALIDATION_ENV) ./$(TESTDIR)/test_window_lifecycle
