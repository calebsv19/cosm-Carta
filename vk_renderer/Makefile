# ===== Makefile: macOS + Vulkan (MoltenVK) + SDL2 =====

# Tools
CC         ?= cc
AR         ?= ar
PKG_CONFIG ?= pkg-config

# Flags
CSTD   ?= -std=c11
CWARN  ?= -Wall -Wextra -Wpedantic
COPT   ?= -O2

# Project includes
INC := -Iinclude

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
APPLE_FW := -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo

# Common compile/link flags
CFLAGS  := $(COPT) $(CSTD) $(CWARN) $(INC) $(VULKAN_CFLAGS) $(SDL_CFLAGS) -D_THREAD_SAFE -DVK_USE_PLATFORM_METAL_EXT -DVK_RENDERER_SHADER_ROOT=\"$(CURDIR)\"
LDFLAGS := $(VULKAN_LIBS) $(SDL_LIBS) $(APPLE_FW)

# Sources / objects
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/obj/%.o,$(SRC))

# Outputs
LIBDIR := build/lib
BINDIR := build/example
LIB    := $(LIBDIR)/libvkrenderer.a

# Default target
.PHONY: all
all: $(LIB)

# Build static library from all src/*.c
$(LIB): $(OBJ)
	@mkdir -p $(LIBDIR)
	$(AR) rcs $@ $(OBJ)

# Compile .c -> .o
build/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Example (optional)
.PHONY: example
example: $(LIB)
	@test -f examples/sdl_vulkan_example.c || { echo "No examples/sdl_vulkan_example.c present; skipping."; exit 0; }
	@mkdir -p $(BINDIR)
	$(CC) examples/sdl_vulkan_example.c $(CFLAGS) -L$(LIBDIR) -lvkrenderer -o $(BINDIR)/sdl_vulkan_example $(LDFLAGS)
	@echo "Built $(BINDIR)/sdl_vulkan_example"

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
