HOST_CC ?= cc
FISICS_CC ?= /Users/calebsv/Desktop/CodeWork/fisiCs/fisics
BUILD_TOOLCHAIN ?= clang
PACKAGE_TOOLCHAIN ?= $(BUILD_TOOLCHAIN)
TEST_TOOLCHAIN ?= clang
RELEASE_TOOLCHAIN ?= clang
PKG_CONFIG ?= pkg-config
UNAME_S := $(shell uname -s)
TARGET_CONTRACT_HELPER ?= ../bin/desktop_release_target_contract.sh
HOST_ARCH := $(shell uname -m)
TARGET_OS ?= $(UNAME_S)
TARGET_ARCH ?= $(HOST_ARCH)
TARGET_VARIANT ?= desktop-app
TARGET_TRIPLE ?= $(TARGET_OS)-$(TARGET_ARCH)
RELEASE_PLATFORM ?= $(TARGET_OS)
RELEASE_ARCH ?= $(TARGET_ARCH)
TARGET_HOMEBREW_PREFIX :=
TARGET_ALT_HOMEBREW_PREFIX :=
TARGET_PKG_CONFIG_LIBDIR :=
TARGET_DEP_SEARCH_ROOTS :=
ARCH_FLAGS :=
SUPPORTED_TOOLCHAINS := clang fisics

ifeq ($(filter $(BUILD_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported BUILD_TOOLCHAIN '$(BUILD_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif
ifeq ($(filter $(PACKAGE_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported PACKAGE_TOOLCHAIN '$(PACKAGE_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif
ifeq ($(filter $(TEST_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported TEST_TOOLCHAIN '$(TEST_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif
ifeq ($(filter $(RELEASE_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported RELEASE_TOOLCHAIN '$(RELEASE_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif

ifeq ($(UNAME_S),Darwin)
TARGET_OS_INPUT := $(TARGET_OS)
TARGET_ARCH_INPUT := $(TARGET_ARCH)
TARGET_VARIANT_INPUT := $(TARGET_VARIANT)
HOST_ARCH := $(strip $(shell "$(TARGET_CONTRACT_HELPER)" get host_arch))
TARGET_OS := $(strip $(shell TARGET_OS="$(TARGET_OS_INPUT)" TARGET_ARCH="$(TARGET_ARCH_INPUT)" TARGET_VARIANT="$(TARGET_VARIANT_INPUT)" "$(TARGET_CONTRACT_HELPER)" get target_os))
TARGET_ARCH := $(strip $(shell TARGET_OS="$(TARGET_OS_INPUT)" TARGET_ARCH="$(TARGET_ARCH_INPUT)" TARGET_VARIANT="$(TARGET_VARIANT_INPUT)" "$(TARGET_CONTRACT_HELPER)" get target_arch))
TARGET_VARIANT := $(strip $(shell TARGET_OS="$(TARGET_OS_INPUT)" TARGET_ARCH="$(TARGET_ARCH_INPUT)" TARGET_VARIANT="$(TARGET_VARIANT_INPUT)" "$(TARGET_CONTRACT_HELPER)" get target_variant))
TARGET_TRIPLE := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get target_triple))
RELEASE_PLATFORM := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get release_platform))
RELEASE_ARCH := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get release_arch))
TARGET_HOMEBREW_PREFIX := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get homebrew_prefix))
TARGET_ALT_HOMEBREW_PREFIX := $(strip $(shell TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(TARGET_CONTRACT_HELPER)" get alt_homebrew_prefix))
TARGET_PKG_CONFIG_LIBDIR := $(TARGET_HOMEBREW_PREFIX)/lib/pkgconfig:$(TARGET_HOMEBREW_PREFIX)/share/pkgconfig
TARGET_DEP_SEARCH_ROOTS := $(TARGET_HOMEBREW_PREFIX):$(TARGET_ALT_HOMEBREW_PREFIX)
ARCH_FLAGS := -arch $(TARGET_ARCH)
endif

APP_CC := $(if $(filter fisics,$(BUILD_TOOLCHAIN)),$(FISICS_CC),$(HOST_CC))
APP_COMPILER_DEP := $(if $(filter fisics,$(BUILD_TOOLCHAIN)),$(FISICS_CC),)
BUILD_ROOT := build
TARGET_BUILD_ROOT := $(BUILD_ROOT)/targets/$(TARGET_TRIPLE)
HOST_BUILD_ROOT := $(TARGET_BUILD_ROOT)/host
APP_TOOLCHAIN_ROOT := $(TARGET_BUILD_ROOT)/toolchains/$(BUILD_TOOLCHAIN)
APP_OBJ_DIR := $(APP_TOOLCHAIN_ROOT)/obj
APP_BIN_DIR := $(APP_TOOLCHAIN_ROOT)/bin
APP_BIN := $(APP_BIN_DIR)/mapforge
APP_COMPILER_STAMP := $(APP_TOOLCHAIN_ROOT)/.compiler_stamp
PACKAGE_BIN := $(TARGET_BUILD_ROOT)/toolchains/$(PACKAGE_TOOLCHAIN)/bin/mapforge
TEST_APP_BIN := $(TARGET_BUILD_ROOT)/toolchains/$(TEST_TOOLCHAIN)/bin/mapforge
SHARED_BUILD_DIR := $(TARGET_BUILD_ROOT)/shared
TOOL_BIN_DIR := $(TARGET_BUILD_ROOT)/tools
TEST_BIN_DIR := $(TARGET_BUILD_ROOT)/tests

# Build the application when running plain `make`.
.DEFAULT_GOAL := app

SDL_CFLAGS :=
SDL_LIBS :=
SDL_TTF_CFLAGS :=
SDL_TTF_LIBS :=
VULKAN_CFLAGS :=
VULKAN_LIBS :=
JSON_CFLAGS :=
JSON_LIBS :=
SQLITE_CFLAGS :=
SQLITE_LIBS :=
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
CORE_VIEWPORT2D_DIR := $(SHARED_ROOT)/core/core_viewport2d
KIT_RUNTIME_DIAG_DIR := $(SHARED_ROOT)/kit/kit_runtime_diag
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
SHARED_CC := $(HOST_CC) $(ARCH_FLAGS)

CORE_SPACE_LIB := $(SHARED_BUILD_DIR)/libcore_space.a
CORE_BASE_LIB := $(SHARED_BUILD_DIR)/libcore_base.a
CORE_IO_LIB := $(SHARED_BUILD_DIR)/libcore_io.a
CORE_DATA_LIB := $(SHARED_BUILD_DIR)/libcore_data.a
CORE_PACK_LIB := $(SHARED_BUILD_DIR)/libcore_pack.a
CORE_TIME_LIB := $(SHARED_BUILD_DIR)/libcore_time.a
CORE_QUEUE_LIB := $(SHARED_BUILD_DIR)/libcore_queue.a
CORE_SCHED_LIB := $(SHARED_BUILD_DIR)/libcore_sched.a
CORE_JOBS_LIB := $(SHARED_BUILD_DIR)/libcore_jobs.a
CORE_WORKERS_LIB := $(SHARED_BUILD_DIR)/libcore_workers.a
CORE_WAKE_LIB := $(SHARED_BUILD_DIR)/libcore_wake.a
CORE_KERNEL_LIB := $(SHARED_BUILD_DIR)/libcore_kernel.a
CORE_TRACE_LIB := $(SHARED_BUILD_DIR)/libcore_trace.a
CORE_THEME_LIB := $(SHARED_BUILD_DIR)/libcore_theme.a
CORE_FONT_LIB := $(SHARED_BUILD_DIR)/libcore_font.a
CORE_VIEWPORT2D_LIB := $(SHARED_BUILD_DIR)/libcore_viewport2d.a
KIT_RUNTIME_DIAG_LIB := $(SHARED_BUILD_DIR)/libkit_runtime_diag.a
KIT_RENDER_EXTERNAL_TEXT_OBJ := $(HOST_BUILD_ROOT)/kit_render/kit_render_external_text.o

VK_RENDERER_DIR ?= $(SHARED_ROOT)/vk_renderer
VK_RENDERER_RESOLVED_DIR := $(VK_RENDERER_DIR)
VK_RENDERER_INCLUDE := $(VK_RENDERER_RESOLVED_DIR)/include
VK_RENDERER_STATIC_LIB := $(VK_RENDERER_RESOLVED_DIR)/build/lib/libvkrenderer.a
VK_RENDERER_SRCS := $(wildcard $(VK_RENDERER_RESOLVED_DIR)/src/*.c)
VK_RENDERER_OBJS := $(patsubst $(VK_RENDERER_RESOLVED_DIR)/src/%.c,$(HOST_BUILD_ROOT)/vk_renderer/%.o,$(VK_RENDERER_SRCS))
VK_BUILD_LIB := $(TARGET_BUILD_ROOT)/vk/lib/libvkrenderer.a
VK_BUILD_SHADER_DIR := $(TARGET_BUILD_ROOT)/vk/shaders
VK_REQUIRED_SHADERS := fill.vert.spv fill.frag.spv line.vert.spv line.frag.spv textured.vert.spv textured.frag.spv
VK_APP_ENABLED := $(if $(wildcard $(VK_RENDERER_INCLUDE)/vk_renderer.h),1,)

ifneq ($(UNAME_S),Darwin)
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SDL_TTF_CFLAGS := $(shell $(PKG_CONFIG) --cflags SDL2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell $(PKG_CONFIG) --libs SDL2_ttf 2>/dev/null)
VULKAN_CFLAGS := $(shell $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(shell $(PKG_CONFIG) --libs vulkan 2>/dev/null)
JSON_CFLAGS := $(shell $(PKG_CONFIG) --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell $(PKG_CONFIG) --libs json-c 2>/dev/null)
SQLITE_CFLAGS := $(shell $(PKG_CONFIG) --cflags sqlite3 2>/dev/null)
SQLITE_LIBS := $(shell $(PKG_CONFIG) --libs sqlite3 2>/dev/null)
else
SDL_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs sdl2 2>/dev/null)
SDL_TTF_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags SDL2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs SDL2_ttf 2>/dev/null)
VULKAN_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs vulkan 2>/dev/null)
JSON_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs json-c 2>/dev/null)
SQLITE_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sqlite3 2>/dev/null)
SQLITE_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs sqlite3 2>/dev/null)
endif

ifeq ($(strip $(SDL_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/SDL2/SDL.h),)
SDL_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/SDL2/SDL.h),)
SDL_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
endif
endif

ifeq ($(strip $(SDL_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libSDL2.dylib),)
SDL_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lSDL2
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libSDL2.dylib),)
SDL_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lSDL2
else
SDL_LIBS := -lSDL2
endif
endif

ifeq ($(strip $(SDL_TTF_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/SDL2/SDL_ttf.h),)
SDL_TTF_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/SDL2/SDL_ttf.h),)
SDL_TTF_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
endif
endif

ifeq ($(strip $(SDL_TTF_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libSDL2_ttf.dylib),)
SDL_TTF_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lSDL2_ttf
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libSDL2_ttf.dylib),)
SDL_TTF_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lSDL2_ttf
else
SDL_TTF_LIBS := -lSDL2_ttf
endif
endif

ifeq ($(strip $(VULKAN_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/vulkan/vulkan.h),)
VULKAN_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/vulkan/vulkan.h),)
VULKAN_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include
endif
endif

ifeq ($(strip $(VULKAN_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libvulkan.1.dylib),)
VULKAN_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lvulkan
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libvulkan.1.dylib),)
VULKAN_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lvulkan
else
VULKAN_LIBS := -lvulkan
endif
endif

ifeq ($(strip $(JSON_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/json-c/json.h),)
JSON_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/json-c/json.h),)
JSON_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include
endif
endif

ifeq ($(strip $(JSON_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libjson-c.dylib),)
JSON_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -ljson-c
else ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libjson-c.5.dylib),)
JSON_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -ljson-c
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libjson-c.dylib),)
JSON_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -ljson-c
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libjson-c.5.dylib),)
JSON_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -ljson-c
endif
endif

ifeq ($(strip $(SQLITE_CFLAGS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/sqlite3.h),)
SQLITE_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/sqlite3.h),)
SQLITE_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include
endif
endif

ifeq ($(strip $(SQLITE_LIBS)),)
ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/opt/sqlite/lib/libsqlite3.dylib),)
SQLITE_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/opt/sqlite/lib -lsqlite3
else ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libsqlite3.dylib),)
SQLITE_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lsqlite3
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/opt/sqlite/lib/libsqlite3.dylib),)
SQLITE_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/opt/sqlite/lib -lsqlite3
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libsqlite3.dylib),)
SQLITE_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lsqlite3
endif
endif

COMMON_CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2 -g -pthread $(ARCH_FLAGS) $(SDL_CFLAGS) $(SDL_TTF_CFLAGS) $(VULKAN_CFLAGS)
APP_CFLAGS := $(COMMON_CFLAGS)
HOST_CFLAGS := $(COMMON_CFLAGS)
LDLIBS := $(SDL_LIBS) $(SDL_TTF_LIBS) $(JSON_LIBS) -pthread
TOOL_LDLIBS := -lm $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)

ifeq ($(JSON_LIBS),)
LDLIBS += -ljson-c
endif
APP_CFLAGS += $(JSON_CFLAGS)
HOST_CFLAGS += $(JSON_CFLAGS)
ifneq ($(strip $(SQLITE_LIBS)),)
APP_CFLAGS += $(SQLITE_CFLAGS) -DMAPFORGE_HAVE_SQLITE=1
HOST_CFLAGS += $(SQLITE_CFLAGS) -DMAPFORGE_HAVE_SQLITE=1
LDLIBS += $(SQLITE_LIBS)
TOOL_LDLIBS += $(SQLITE_LIBS)
endif
APP_CFLAGS += -I$(CORE_SPACE_DIR)/include
APP_CFLAGS += -I$(CORE_BASE_DIR)/include
APP_CFLAGS += -I$(CORE_IO_DIR)/include
APP_CFLAGS += -I$(CORE_DATA_DIR)/include
APP_CFLAGS += -I$(CORE_PACK_DIR)/include
APP_CFLAGS += -I$(CORE_TIME_DIR)/include
APP_CFLAGS += -I$(CORE_QUEUE_DIR)/include
APP_CFLAGS += -I$(CORE_SCHED_DIR)/include
APP_CFLAGS += -I$(CORE_JOBS_DIR)/include
APP_CFLAGS += -I$(CORE_WORKERS_DIR)/include
APP_CFLAGS += -I$(CORE_WAKE_DIR)/include
APP_CFLAGS += -I$(CORE_KERNEL_DIR)/include
APP_CFLAGS += -I$(CORE_TRACE_DIR)/include
APP_CFLAGS += -I$(CORE_THEME_DIR)/include
APP_CFLAGS += -I$(CORE_FONT_DIR)/include
APP_CFLAGS += -I$(CORE_VIEWPORT2D_DIR)/include
APP_CFLAGS += -I$(KIT_RUNTIME_DIAG_DIR)/include
APP_CFLAGS += -I$(KIT_RENDER_DIR)/include
HOST_CFLAGS += -I$(CORE_SPACE_DIR)/include
HOST_CFLAGS += -I$(CORE_BASE_DIR)/include
HOST_CFLAGS += -I$(CORE_IO_DIR)/include
HOST_CFLAGS += -I$(CORE_DATA_DIR)/include
HOST_CFLAGS += -I$(CORE_PACK_DIR)/include
HOST_CFLAGS += -I$(CORE_TIME_DIR)/include
HOST_CFLAGS += -I$(CORE_QUEUE_DIR)/include
HOST_CFLAGS += -I$(CORE_SCHED_DIR)/include
HOST_CFLAGS += -I$(CORE_JOBS_DIR)/include
HOST_CFLAGS += -I$(CORE_WORKERS_DIR)/include
HOST_CFLAGS += -I$(CORE_WAKE_DIR)/include
HOST_CFLAGS += -I$(CORE_KERNEL_DIR)/include
HOST_CFLAGS += -I$(CORE_TRACE_DIR)/include
HOST_CFLAGS += -I$(CORE_THEME_DIR)/include
HOST_CFLAGS += -I$(CORE_FONT_DIR)/include
HOST_CFLAGS += -I$(CORE_VIEWPORT2D_DIR)/include
HOST_CFLAGS += -I$(KIT_RUNTIME_DIAG_DIR)/include
HOST_CFLAGS += -I$(KIT_RENDER_DIR)/include

SRCS := $(shell find src -name '*.c')
OBJS := $(patsubst src/%.c,$(APP_OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
DEPS += $(KIT_RENDER_EXTERNAL_TEXT_OBJ:.o=.d)
DEPS += $(VK_RENDERER_OBJS:.o=.d)
LINK_OBJS := $(OBJS)
CORE_SHARED_LIBS := $(CORE_TRACE_LIB) $(CORE_PACK_LIB) $(CORE_KERNEL_LIB) $(CORE_WAKE_LIB) $(CORE_WORKERS_LIB) $(CORE_JOBS_LIB) $(CORE_SCHED_LIB) $(CORE_QUEUE_LIB) $(CORE_TIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_VIEWPORT2D_LIB) $(KIT_RUNTIME_DIAG_LIB) $(CORE_SPACE_LIB) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
LINK_OBJS += $(KIT_RENDER_EXTERNAL_TEXT_OBJ)
LINK_OBJS += $(CORE_SHARED_LIBS)
TARGET := $(APP_BIN)
DIST_DIR := $(TARGET_BUILD_ROOT)/dist
PACKAGE_APP_NAME := Carta.app
PACKAGE_APP_DIR := $(DIST_DIR)/$(PACKAGE_APP_NAME)
PACKAGE_CONTENTS_DIR := $(PACKAGE_APP_DIR)/Contents
PACKAGE_MACOS_DIR := $(PACKAGE_CONTENTS_DIR)/MacOS
PACKAGE_RESOURCES_DIR := $(PACKAGE_CONTENTS_DIR)/Resources
PACKAGE_FRAMEWORKS_DIR := $(PACKAGE_CONTENTS_DIR)/Frameworks
PACKAGE_TOOLS_DIR := $(PACKAGE_RESOURCES_DIR)/tools
PACKAGE_INFO_PLIST_SRC := tools/packaging/macos/Info.plist
PACKAGE_LAUNCHER_SRC := tools/packaging/macos/mapforge-launcher
PACKAGE_DYLIB_BUNDLER := tools/packaging/macos/bundle-dylibs.sh
PACKAGE_APP_ICON_NAME := AppIcon
PACKAGE_APP_ICON_FILE := $(PACKAGE_APP_ICON_NAME).icns
PACKAGE_LOCAL_ICON_DIR := tools/packaging/macos/local_app_icon
PACKAGE_APP_ICON_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_FILE)
PACKAGE_APP_ICONSET_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_NAME).iconset
PACKAGE_BUNDLED_ICON_PATH := $(PACKAGE_RESOURCES_DIR)/$(PACKAGE_APP_ICON_FILE)
DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(PACKAGE_APP_NAME)
PACKAGE_ADHOC_SIGN_IDENTITY ?= -
PACKAGED_HELPER_TOOL_NAMES := mapforge_region mapforge_region_validate mapforge_graph
PACKAGED_HELPER_TOOLS := $(foreach tool,$(PACKAGED_HELPER_TOOL_NAMES),$(PACKAGE_TOOLS_DIR)/$(tool))

# RL0 release contract (pilot lock).
RELEASE_VERSION_FILE ?= VERSION
RELEASE_VERSION ?= $(strip $(shell cat "$(RELEASE_VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
RELEASE_CHANNEL ?= stable
RELEASE_PRODUCT_NAME := Carta
RELEASE_PROGRAM_KEY := map_forge
RELEASE_BUNDLE_ID := com.cosm.carta
RELEASE_ARTIFACT_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(RELEASE_PLATFORM)-$(RELEASE_ARCH)-$(RELEASE_CHANNEL)
RELEASE_DIR := build/release
RELEASE_APP_ZIP := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).zip
RELEASE_MANIFEST := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).manifest.txt
RELEASE_CODESIGN_IDENTITY ?= $(if $(strip $(APPLE_SIGN_IDENTITY)),$(APPLE_SIGN_IDENTITY),$(PACKAGE_ADHOC_SIGN_IDENTITY))
# Export/signing contract variables are intentionally unset by default.
APPLE_SIGN_IDENTITY ?=
APPLE_NOTARY_PROFILE ?=
APPLE_TEAM_ID ?=
STAPLE_MAX_ATTEMPTS ?= 6
STAPLE_RETRY_DELAY_SEC ?= 15
TOOL_TARGET := $(TOOL_BIN_DIR)/mapforge_region
TOOL_SRCS := tools/mapforge_region.c src/map/mercator.c src/map/tile_math.c src/core/log.c
REGION_VALIDATE_TARGET := $(TOOL_BIN_DIR)/mapforge_region_validate
REGION_VALIDATE_SRCS := tools/mapforge_region_validate.c src/app/region.c src/app/region_loader.c src/map/tile_source.c src/core/log.c
GRAPH_TARGET := $(TOOL_BIN_DIR)/mapforge_graph
GRAPH_SRCS := tools/mapforge_graph.c src/map/mercator.c src/core/log.c
MAP_SPACE_TEST_TARGET := $(TEST_BIN_DIR)/map_space_test
MAP_SPACE_TEST_SRCS := tests/map_space_test.c src/map/map_space.c src/map/tile_math.c src/map/mercator.c src/camera/camera.c src/camera/camera_viewport_bridge.c
SHARED_THEME_FONT_ADAPTER_TEST_TARGET := $(TEST_BIN_DIR)/shared_theme_font_adapter_test
SHARED_THEME_FONT_ADAPTER_TEST_SRCS := tests/shared_theme_font_adapter_test.c src/ui/shared_theme_font_adapter.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c $(CORE_BASE_DIR)/src/core_base.c
MAP_TRACE_CONTRACT_TEST_TARGET := $(TEST_BIN_DIR)/map_trace_contract_test
MAP_TRACE_CONTRACT_TEST_SRCS := tests/map_trace_contract_test.c
APP_WORKER_CONTRACT_TEST_TARGET := $(TEST_BIN_DIR)/app_worker_contract_test
APP_WORKER_CONTRACT_TEST_SRCS := tests/app_worker_contract_test.c src/app/app_worker_contract.c
TILE_LOADER_SHUTDOWN_TEST_TARGET := $(TEST_BIN_DIR)/tile_loader_shutdown_test
TILE_LOADER_SHUTDOWN_TEST_SRCS := tests/tile_loader_shutdown_test.c src/map/tile_loader.c src/map/tile_source.c src/map/mft_loader.c src/map/polygon_cache.c src/map/polygon_triangulator.c src/core/log.c
TILE_SOURCE_ARCHIVE_TEST_TARGET := $(TEST_BIN_DIR)/tile_source_archive_test
TILE_SOURCE_ARCHIVE_TEST_SRCS := tests/tile_source_archive_test.c src/map/tile_source.c
APP_ROUTE_SERVICE_TEST_TARGET := $(TEST_BIN_DIR)/app_route_service_test
APP_ROUTE_SERVICE_TEST_SRCS := tests/app_route_service_test.c src/app/route/app_route_service.c
APP_TILE_PRESENTER_POLICY_TEST_TARGET := $(TEST_BIN_DIR)/app_tile_presenter_policy_test
APP_TILE_PRESENTER_POLICY_TEST_SRCS := tests/app_tile_presenter_policy_test.c src/app/app_tile_presenter.c src/app/app_tile_lifecycle.c src/core/time.c
POLYGON_CACHE_GUARDRAILS_TEST_TARGET := $(TEST_BIN_DIR)/polygon_cache_guardrails_test
POLYGON_CACHE_GUARDRAILS_TEST_SRCS := tests/polygon_cache_guardrails_test.c src/map/polygon_cache.c src/map/polygon_triangulator.c
APP_RUNTIME_INPUT_POLICY_TEST_TARGET := $(TEST_BIN_DIR)/app_runtime_input_policy_test
APP_RUNTIME_INPUT_POLICY_TEST_SRCS := tests/app_runtime_input_policy_test.c src/app/app_runtime_input_policy.c
APP_HEADER_LAYER_LAYOUT_TEST_TARGET := $(TEST_BIN_DIR)/app_header_layer_layout_test
APP_HEADER_LAYER_LAYOUT_TEST_SRCS := tests/app_header_layer_layout_test.c src/app/app_header_layer_layout.c
APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET := $(TEST_BIN_DIR)/app_runtime_window_resize_test
APP_RUNTIME_WINDOW_RESIZE_TEST_SRCS := tests/app_runtime_window_resize_test.c src/app/app_runtime_window.c
TILE_MANAGER_RESIDENCY_TEST_TARGET := $(TEST_BIN_DIR)/tile_manager_residency_test
TILE_MANAGER_RESIDENCY_TEST_SRCS := tests/tile_manager_residency_test.c src/map/tile_manager.c src/map/tile_source.c src/map/mft_loader.c src/core/log.c

ifeq ($(VK_APP_ENABLED),1)
APP_CFLAGS += -I$(VK_RENDERER_INCLUDE) -DMAPFORGE_HAVE_VK=1 -DVK_RENDERER_SHADER_ROOT=\"$(VK_RENDERER_RESOLVED_DIR)\"
HOST_CFLAGS += -I$(VK_RENDERER_INCLUDE) -DMAPFORGE_HAVE_VK=1 -DVK_RENDERER_SHADER_ROOT=\"$(VK_RENDERER_RESOLVED_DIR)\"
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
PACKAGE_REGIONS_SRC ?= $(MAPFORGE_REGIONS_DIR)
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
EMIT_ARCHIVE ?= 0
ARCHIVE_PATH ?= tiles.mbtiles

REGION_TOOL_FLAGS := $(if $(filter 1,$(REPLACE)),--replace,) \
	--keep-old $(KEEP_OLD) \
	--prune-days $(PRUNE_DAYS) \
	$(if $(filter 1,$(PRUNE_DRY_RUN)),--prune-dry-run,) \
	$(if $(filter 1,$(PAD_BOUNDS)),--pad-bounds,) \
	$(if $(filter 1,$(EMIT_CONTOUR_EMPTY)),--emit-contour-empty,) \
	$(if $(filter 1,$(EMIT_LEGACY_TILES)),--emit-legacy-tiles,--no-legacy-tiles) \
	$(if $(filter 1,$(EMIT_ARCHIVE)),--emit-archive --archive-path $(ARCHIVE_PATH),)

GRAPH_TOOL_FLAGS := $(if $(filter 1,$(REPLACE)),--replace,) \
	--keep-old $(KEEP_OLD) \
	--prune-days $(PRUNE_DAYS) \
	$(if $(filter 1,$(PRUNE_DRY_RUN)),--prune-dry-run,)

test run-headless-smoke: BUILD_TOOLCHAIN := $(TEST_TOOLCHAIN)
test-%: BUILD_TOOLCHAIN := $(TEST_TOOLCHAIN)
RELEASE_MAKE := $(MAKE) --no-print-directory TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" BUILD_TOOLCHAIN="$(RELEASE_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(RELEASE_TOOLCHAIN)"

app: $(TARGET)

$(SHARED_BUILD_DIR):
	@mkdir -p "$@"

$(CORE_BASE_LIB): | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_BASE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_BASE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_BASE_DIR)/build/libcore_base.a" "$@"

$(CORE_IO_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_IO_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_IO_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_IO_DIR)/build/libcore_io.a" "$@"

$(CORE_DATA_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_DATA_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_DATA_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_DATA_DIR)/build/libcore_data.a" "$@"

$(CORE_SPACE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_SPACE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_SPACE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_SPACE_DIR)/build/libcore_space.a" "$@"

$(CORE_PACK_LIB): $(CORE_IO_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_PACK_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_PACK_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_PACK_DIR)/build/libcore_pack.a" "$@"

$(CORE_TIME_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_TIME_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_TIME_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_TIME_DIR)/build/libcore_time.a" "$@"

$(CORE_QUEUE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_QUEUE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_QUEUE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_QUEUE_DIR)/build/libcore_queue.a" "$@"

$(CORE_SCHED_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_SCHED_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_SCHED_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_SCHED_DIR)/build/libcore_sched.a" "$@"

$(CORE_JOBS_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_JOBS_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_JOBS_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_JOBS_DIR)/build/libcore_jobs.a" "$@"

$(CORE_WORKERS_LIB): $(CORE_QUEUE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_WORKERS_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_WORKERS_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_WORKERS_DIR)/build/libcore_workers.a" "$@"

$(CORE_WAKE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_WAKE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_WAKE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_WAKE_DIR)/build/libcore_wake.a" "$@"

$(CORE_KERNEL_LIB): $(CORE_SCHED_LIB) $(CORE_JOBS_LIB) $(CORE_WAKE_LIB) $(CORE_QUEUE_LIB) $(CORE_TIME_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_KERNEL_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_KERNEL_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_KERNEL_DIR)/build/libcore_kernel.a" "$@"

$(CORE_TRACE_LIB): $(CORE_PACK_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_TRACE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_TRACE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_TRACE_DIR)/build/libcore_trace.a" "$@"

$(CORE_THEME_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_THEME_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_THEME_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_THEME_DIR)/build/libcore_theme.a" "$@"

$(CORE_FONT_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_FONT_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_FONT_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_FONT_DIR)/build/libcore_font.a" "$@"

$(CORE_VIEWPORT2D_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_VIEWPORT2D_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_VIEWPORT2D_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_VIEWPORT2D_DIR)/build/libcore_viewport2d.a" "$@"

$(KIT_RUNTIME_DIAG_LIB): | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(KIT_RUNTIME_DIAG_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(KIT_RUNTIME_DIAG_DIR)" CC="$(SHARED_CC)"
	@cp "$(KIT_RUNTIME_DIAG_DIR)/build/libkit_runtime_diag.a" "$@"

$(APP_COMPILER_STAMP): $(APP_COMPILER_DEP)
	@mkdir -p $(dir $@)
	@touch $@

$(TARGET): $(LINK_OBJS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@ $(LDLIBS)

$(APP_OBJ_DIR)/%.o: src/%.c $(APP_COMPILER_STAMP)
	@mkdir -p $(dir $@)
	$(APP_CC) $(APP_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(HOST_BUILD_ROOT)/vk_renderer/%.o: $(VK_RENDERER_RESOLVED_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_RENDER_EXTERNAL_TEXT_OBJ): $(KIT_RENDER_DIR)/src/kit_render_external_text.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

run: app
	MAPFORGE_RENDER_BACKEND=$(RENDER_BACKEND) MAPFORGE_VK_DEBUG=$(VK_DEBUG) MAPFORGE_REGIONS_DIR="$(MAPFORGE_REGIONS_DIR)" ./$(TARGET)

run-headless-smoke: app test-worker-contract test-route-service test-presentation-stability test-polygon-cache-guardrails test-input-policy test-tile-manager-residency test-phase-d-throughput test-region-validate-strict test-region-validate-contract test-runtime-source-policy test-archive-metrics-rollup test-coverage-metadata-contract
	@echo "map_forge headless smoke passed (non-interactive)"

visual-harness: app
	@echo "visual harness binary ready: $(TARGET)"

package-desktop: tools-build graph-build
	@$(MAKE) --no-print-directory TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" app
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)" "$(PACKAGE_TOOLS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_BIN)" "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/mapforge-launcher"
	@chmod +x "$(PACKAGE_MACOS_DIR)/mapforge-launcher" "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		/usr/bin/iconutil -c icns -o "$(PACKAGE_BUNDLED_ICON_PATH)" "$(PACKAGE_APP_ICONSET_SRC)" || exit 1; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "warning: no app icon source found at $(PACKAGE_APP_ICON_SRC) or $(PACKAGE_APP_ICONSET_SRC)"; \
	fi
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" /bin/sh "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/mapforge-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/assets" "$(PACKAGE_RESOURCES_DIR)/shared/assets" "$(PACKAGE_RESOURCES_DIR)/data/runtime" "$(PACKAGE_RESOURCES_DIR)/data/regions"
	@cp -R assets/fonts "$(PACKAGE_RESOURCES_DIR)/assets/"
	@cp -R config "$(PACKAGE_RESOURCES_DIR)/"
	@cp -R "$(SHARED_ROOT)/assets/fonts" "$(PACKAGE_RESOURCES_DIR)/shared/assets/"
	@cp "$(TOOL_TARGET)" "$(PACKAGE_TOOLS_DIR)/mapforge_region"
	@cp "$(REGION_VALIDATE_TARGET)" "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate"
	@cp "$(GRAPH_TARGET)" "$(PACKAGE_TOOLS_DIR)/mapforge_graph"
	@chmod +x "$(PACKAGE_TOOLS_DIR)/mapforge_region" "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate" "$(PACKAGE_TOOLS_DIR)/mapforge_graph"
	@for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
		PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" /bin/sh "$(PACKAGE_DYLIB_BUNDLER)" "$$helper_tool" "$(PACKAGE_FRAMEWORKS_DIR)"; \
	done
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_RESOLVED_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_RESOLVED_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@echo "Region payload bundling disabled; app ships without embedded region packs."
	@for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-launcher"
	@for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$helper_tool"; \
	done
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/mapforge-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/mapforge-bin" || (echo "Missing mapforge-bin"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1); \
	fi
	@test -f "$(PACKAGE_RESOURCES_DIR)/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing bundled Montserrat"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/app.config.json" || (echo "Missing bundled app config"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_region" || (echo "Missing bundled mapforge_region tool"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate" || (echo "Missing bundled mapforge_region_validate tool"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_graph" || (echo "Missing bundled mapforge_graph tool"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing runtime shader"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime state dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/regions" || (echo "Missing regions dir"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libSDL2-2.0.0.dylib" || (echo "Missing bundled SDL2 dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled MoltenVK dylib"; exit 1)
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		for packaged_bin in "$(PACKAGE_MACOS_DIR)/mapforge-bin" "$(PACKAGE_TOOLS_DIR)/mapforge_region" "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate" "$(PACKAGE_TOOLS_DIR)/mapforge_graph"; do \
			actual_archs="$$(/usr/bin/lipo -archs "$$packaged_bin" 2>/dev/null || true)"; \
			case "$$actual_archs" in \
				*"$(TARGET_ARCH)"*) ;; \
				*) echo "arch mismatch for $$packaged_bin: expected $(TARGET_ARCH), got '$$actual_archs'"; exit 1 ;; \
			esac; \
		done; \
	fi
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)" || (echo "codesign verification failed"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/mapforge-launcher" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop app sync complete."

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(DESKTOP_APP_DIR)"
	@echo "Removed desktop copy at $(DESKTOP_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

release-contract:
	@echo "Release contract:"
	@echo "  product_name: $(RELEASE_PRODUCT_NAME)"
	@echo "  program_key: $(RELEASE_PROGRAM_KEY)"
	@echo "  version_file: $(RELEASE_VERSION_FILE)"
	@echo "  version: $(RELEASE_VERSION)"
	@echo "  channel: $(RELEASE_CHANNEL)"
	@echo "  bundle_id: $(RELEASE_BUNDLE_ID)"
	@echo "  artifact_base: $(RELEASE_ARTIFACT_BASENAME)"
	@echo "  release_codesign_identity: $(RELEASE_CODESIGN_IDENTITY)"
	@echo "  sign_identity_set: $$( [ -n \"$(APPLE_SIGN_IDENTITY)\" ] && echo yes || echo no )"
	@echo "  notary_profile_set: $$( [ -n \"$(APPLE_NOTARY_PROFILE)\" ] && echo yes || echo no )"
	@echo "  team_id_set: $$( [ -n \"$(APPLE_TEAM_ID)\" ] && echo yes || echo no )"

release-clean:
	@rm -rf "$(RELEASE_DIR)"
	@echo "Release output cleaned: $(RELEASE_DIR)"

release-build:
	@$(RELEASE_MAKE) release-build-internal

release-bundle-audit:
	@$(RELEASE_MAKE) release-bundle-audit-internal

release-sign:
	@$(RELEASE_MAKE) release-sign-internal

release-verify:
	@$(RELEASE_MAKE) release-verify-internal

release-verify-signed:
	@$(RELEASE_MAKE) release-verify-signed-internal

release-notarize:
	@$(RELEASE_MAKE) release-notarize-internal

release-staple:
	@$(RELEASE_MAKE) release-staple-internal

release-verify-notarized:
	@$(RELEASE_MAKE) release-verify-notarized-internal

release-artifact:
	@$(RELEASE_MAKE) release-artifact-internal

release-distribute:
	@$(RELEASE_MAKE) release-distribute-internal

release-desktop-refresh:
	@$(RELEASE_MAKE) release-desktop-refresh-internal

release-build-internal: clean app
	@echo "Release build complete: $(TARGET)"

release-bundle-audit-internal: package-desktop-self-test
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "bundle id mismatch: expected $(RELEASE_BUNDLE_ID), got $$(cat "$(RELEASE_DIR)/bundle_id.txt")"; exit 1)
	@env -i HOME="$(HOME)" PATH="$(PATH)" "$(PACKAGE_MACOS_DIR)/mapforge-launcher" --print-config > "$(RELEASE_DIR)/print_config.txt"
	@runtime_dir="$$(/usr/bin/grep '^MAPFORGE_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$runtime_dir" ]; then echo "runtime dir missing from print-config"; exit 1; fi; \
	case "$$runtime_dir" in *"/Contents/Resources"*) echo "runtime dir incorrectly points into app bundle: $$runtime_dir"; exit 1;; esac; \
	case "$$runtime_dir" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "runtime dir is not user-writable rooted: $$runtime_dir"; exit 1;; esac
	@theme_path="$$(/usr/bin/grep '^MAPFORGE_THEME_PERSIST_PATH=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$theme_path" ]; then echo "theme persist path missing from print-config"; exit 1; fi; \
	case "$$theme_path" in *"/Contents/Resources"*) echo "theme persist path incorrectly points into app bundle: $$theme_path"; exit 1;; esac; \
	case "$$theme_path" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "theme persist path is not user-writable rooted: $$theme_path"; exit 1;; esac
	@/usr/bin/grep -q '^MAPFORGE_SELECTED_REGIONS_REASON=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing regions-selection diagnostics"; exit 1)
	@otool -L "$(PACKAGE_MACOS_DIR)/mapforge-bin" > "$(RELEASE_DIR)/otool_mapforge_bin.txt"
	@if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_mapforge_bin.txt"; then \
		echo "non-portable dylib dependency detected in $(PACKAGE_MACOS_DIR)/mapforge-bin"; \
		cat "$(RELEASE_DIR)/otool_mapforge_bin.txt"; \
		exit 1; \
	fi
	@for file in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		base="$$(/usr/bin/basename "$$file")"; \
		otool -L "$$file" > "$(RELEASE_DIR)/otool_$$base.txt" || exit 1; \
		if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_$$base.txt"; then \
			echo "non-portable dylib dependency detected in $$file"; \
			cat "$(RELEASE_DIR)/otool_$$base.txt"; \
			exit 1; \
		fi; \
	done
	@for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
		base="$$(/usr/bin/basename "$$helper_tool")"; \
		otool -L "$$helper_tool" > "$(RELEASE_DIR)/otool_$$base.txt" || exit 1; \
		if /usr/bin/grep -Eq '/opt/homebrew|/usr/local|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_$$base.txt"; then \
			echo "non-portable helper-tool dependency detected in $$helper_tool"; \
			cat "$(RELEASE_DIR)/otool_$$base.txt"; \
			exit 1; \
		fi; \
	done
	@echo "release-bundle-audit passed."

release-sign-internal: release-bundle-audit-internal
	@echo "Signing with identity: $(RELEASE_CODESIGN_IDENTITY)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-launcher"; \
		for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$helper_tool"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"; \
	else \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/mapforge-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/mapforge-launcher"; \
		for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$$helper_tool"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_APP_DIR)"; \
	fi
	@echo "release-sign complete."

release-verify-internal:
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-verify note: ad-hoc identity in use; skipping spctl Gatekeeper assessment"; \
	else \
		spctl_output="$$(spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)" 2>&1)"; \
		spctl_status=$$?; \
		if [ $$spctl_status -ne 0 ]; then \
			if printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "internal error in Code Signing subsystem"; then \
				echo "release-verify note: spctl internal subsystem error on this host; codesign verification remains authoritative"; \
			else \
				printf '%s\n' "$$spctl_output"; \
				exit $$spctl_status; \
			fi; \
		else \
			printf '%s\n' "$$spctl_output"; \
		fi; \
	fi
	@echo "release-verify passed."

release-verify-signed-internal: release-sign-internal release-verify-internal
	@echo "release-verify-signed passed."

release-notarize-internal: release-sign-internal
	@if [ -z "$(APPLE_NOTARY_PROFILE)" ]; then \
		echo "APPLE_NOTARY_PROFILE is required for release-notarize"; \
		exit 1; \
	fi
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-notarize requires a real Developer ID signing identity (APPLE_SIGN_IDENTITY)"; \
		exit 1; \
	fi
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@submission_json="$$(xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json)"; \
	echo "$$submission_json" > "$(RELEASE_DIR)/notary_submit.json"; \
	status="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*"status"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | /usr/bin/tail -n 1)"; \
	if [ "$$status" != "Accepted" ]; then \
		submission_id="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*"id"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | /usr/bin/head -n 1)"; \
		echo "release-notarize failed: status=$$status id=$$submission_id"; \
		if [ -n "$$submission_id" ]; then \
			xcrun notarytool log "$$submission_id" --keychain-profile "$(APPLE_NOTARY_PROFILE)" > "$(RELEASE_DIR)/notary_log_$$submission_id.json" || true; \
			echo "notary log: $(RELEASE_DIR)/notary_log_$$submission_id.json"; \
		fi; \
		exit 1; \
	fi
	@echo "release-notarize passed."

release-staple-internal:
	@attempt=1; \
	while [ $$attempt -le "$(STAPLE_MAX_ATTEMPTS)" ]; do \
		if xcrun stapler staple "$(PACKAGE_APP_DIR)"; then \
			break; \
		fi; \
		if [ $$attempt -eq "$(STAPLE_MAX_ATTEMPTS)" ]; then \
			echo "release-staple failed after $$attempt attempts"; \
			exit 1; \
		fi; \
		echo "release-staple retry $$attempt/$(STAPLE_MAX_ATTEMPTS) in $(STAPLE_RETRY_DELAY_SEC)s"; \
		sleep "$(STAPLE_RETRY_DELAY_SEC)"; \
		attempt=$$((attempt + 1)); \
	done
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-staple passed."

release-verify-notarized-internal: release-verify-internal
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-verify-notarized passed."

release-artifact-internal:
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@shasum -a 256 "$(RELEASE_APP_ZIP)" > "$(RELEASE_APP_ZIP).sha256"
	@{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(RELEASE_PROGRAM_KEY)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "platform=$(RELEASE_PLATFORM)"; \
		echo "arch=$(RELEASE_ARCH)"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "artifact=$(RELEASE_APP_ZIP)"; \
		echo "sha256_file=$(RELEASE_APP_ZIP).sha256"; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

release-distribute-internal: release-notarize-internal release-staple-internal release-verify-notarized-internal release-artifact-internal
	@echo "release-distribute passed."

release-desktop-refresh-internal: package-desktop
	@if [ ! -d "$(PACKAGE_APP_DIR)" ]; then \
		echo "release-desktop-refresh requires an existing built app at $(PACKAGE_APP_DIR)"; \
		echo "run release-distribute first"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Release app refreshed at $(DESKTOP_APP_DIR)"

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

tools:
	tools/run_with_progress.sh --label "make tools" $(MAKE) --no-print-directory tools-build

tools-build: $(TOOL_TARGET) $(REGION_VALIDATE_TARGET)

$(TOOL_TARGET): $(TOOL_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TOOL_SRCS) -o $@ $(TOOL_LDLIBS)

$(REGION_VALIDATE_TARGET): $(REGION_VALIDATE_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(REGION_VALIDATE_SRCS) -o $@ $(TOOL_LDLIBS) $(if $(JSON_LIBS),$(JSON_LIBS),-ljson-c)

graph:
	tools/run_with_progress.sh --label "make graph" $(MAKE) --no-print-directory graph-build

graph-build: $(GRAPH_TARGET)

$(GRAPH_TARGET): $(GRAPH_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(GRAPH_SRCS) -o $@ $(TOOL_LDLIBS)

test-space: $(MAP_SPACE_TEST_TARGET)
	./$(MAP_SPACE_TEST_TARGET)

build-safety-check: tools graph
	./tests/test_build_safety.sh

test: test-space build-safety-check
test: test-region-validate-strict
test: test-region-validate-contract
test: test-runtime-source-policy
test: test-archive-metrics-rollup
test: test-coverage-metadata-contract
test: test-trace-contract
test: test-worker-contract
test: test-tile-loader-shutdown
test: test-tile-source-archive
test: test-route-service
test: test-tile-presenter-policy
test: test-presentation-stability
test: test-input-policy
test: test-tile-manager-residency
test: test-phase-d-throughput

test-region-validate-strict: tools-build
	./tests/test_region_validate_strict.sh

test-region-validate-contract: tools-build
	./tests/test_region_validate_contract.sh

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

test-tile-presenter-policy: $(APP_TILE_PRESENTER_POLICY_TEST_TARGET)
	./$(APP_TILE_PRESENTER_POLICY_TEST_TARGET)

test-presentation-stability: $(APP_TILE_PRESENTER_POLICY_TEST_TARGET)
	./$(APP_TILE_PRESENTER_POLICY_TEST_TARGET)

test-polygon-cache-guardrails: $(POLYGON_CACHE_GUARDRAILS_TEST_TARGET)
	./$(POLYGON_CACHE_GUARDRAILS_TEST_TARGET)

test-input-policy: $(APP_RUNTIME_INPUT_POLICY_TEST_TARGET)
	./$(APP_RUNTIME_INPUT_POLICY_TEST_TARGET)

test-header-layer-layout: $(APP_HEADER_LAYER_LAYOUT_TEST_TARGET)
	./$(APP_HEADER_LAYER_LAYOUT_TEST_TARGET)

test-window-resize: $(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET)
	./$(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET)

test-tile-manager-residency: $(TILE_MANAGER_RESIDENCY_TEST_TARGET)
	./$(TILE_MANAGER_RESIDENCY_TEST_TARGET)

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
	MAPFORGE_BINARY="$(TEST_APP_BIN)" MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_SEATTLE=8 \
	MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_SEATTLE=1200 \
	MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_DOWNTOWN=10 \
	MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_DOWNTOWN=800 \
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
	MAPFORGE_PHASE_D2_MAX_MIN_COV_DROP=0.010 \
	MAPFORGE_PHASE_D2_MAX_FB_RATIO_DELTA=0.001 \
	MAPFORGE_PHASE_D2_MAX_CHURN_BAND_DELTA=0 \
	MAPFORGE_PHASE_D2_MAX_CHURN_QUEUE_DELTA=0 \
	MAPFORGE_PHASE_D2_MAX_BAND_FB_PEAK_DELTA=0 \
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

$(APP_TILE_PRESENTER_POLICY_TEST_TARGET): $(APP_TILE_PRESENTER_POLICY_TEST_SRCS) $(CORE_TIME_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_TILE_PRESENTER_POLICY_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_TIME_LIB)

$(POLYGON_CACHE_GUARDRAILS_TEST_TARGET): $(POLYGON_CACHE_GUARDRAILS_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(POLYGON_CACHE_GUARDRAILS_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_RUNTIME_INPUT_POLICY_TEST_TARGET): $(APP_RUNTIME_INPUT_POLICY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_RUNTIME_INPUT_POLICY_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_HEADER_LAYER_LAYOUT_TEST_TARGET): $(APP_HEADER_LAYER_LAYOUT_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_HEADER_LAYER_LAYOUT_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET): $(APP_RUNTIME_WINDOW_RESIZE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(APP_RUNTIME_WINDOW_RESIZE_TEST_SRCS) -o $@ $(TOOL_LDLIBS)

$(TILE_MANAGER_RESIDENCY_TEST_TARGET): $(TILE_MANAGER_RESIDENCY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TILE_MANAGER_RESIDENCY_TEST_SRCS) -o $@ $(TOOL_LDLIBS) $(CORE_SHARED_LIBS)

route: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" $(GRAPH_TOOL_FLAGS)

region: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

region-archive:
	$(MAKE) --no-print-directory region EMIT_ARCHIVE=1 EMIT_LEGACY_TILES=0

region-validate: tools-build
	./$(REGION_VALIDATE_TARGET) $(if $(REGION),--region $(REGION),) $(if $(filter 1,$(STRICT)),--strict,)

region-rebuild: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) --replace $(REGION_TOOL_FLAGS)

region-rebuild-archive:
	$(MAKE) --no-print-directory region-rebuild EMIT_ARCHIVE=1 EMIT_LEGACY_TILES=0

route-rebuild: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" --replace $(GRAPH_TOOL_FLAGS)

tools-progress: tools

graph-progress: graph

region-progress:
	tools/run_with_progress.sh --label "region $(REGION)" ./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

region-progress-archive:
	$(MAKE) --no-print-directory region-progress EMIT_ARCHIVE=1 EMIT_LEGACY_TILES=0

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
	@for path in "$(CORE_BASE_LIB)" "$(CORE_IO_LIB)" "$(CORE_DATA_LIB)" "$(CORE_SPACE_LIB)" "$(CORE_PACK_LIB)" "$(CORE_TIME_LIB)" "$(CORE_QUEUE_LIB)" "$(CORE_SCHED_LIB)" "$(CORE_JOBS_LIB)" "$(CORE_WORKERS_LIB)" "$(CORE_WAKE_LIB)" "$(CORE_KERNEL_LIB)" "$(CORE_TRACE_LIB)" "$(CORE_VIEWPORT2D_LIB)"; do \
		if [ ! -f "$$path" ]; then \
			echo "missing: $$path"; \
			exit 1; \
		fi; \
		echo "ok: $$path"; \
	done
	@echo ""
	@echo "=== Shared Versions ==="
	@for path in "$(CORE_BASE_DIR)/VERSION" "$(CORE_IO_DIR)/VERSION" "$(CORE_DATA_DIR)/VERSION" "$(CORE_SPACE_DIR)/VERSION" "$(CORE_PACK_DIR)/VERSION" "$(CORE_TIME_DIR)/VERSION" "$(CORE_QUEUE_DIR)/VERSION" "$(CORE_SCHED_DIR)/VERSION" "$(CORE_JOBS_DIR)/VERSION" "$(CORE_WORKERS_DIR)/VERSION" "$(CORE_WAKE_DIR)/VERSION" "$(CORE_KERNEL_DIR)/VERSION" "$(CORE_TRACE_DIR)/VERSION" "$(CORE_VIEWPORT2D_DIR)/VERSION"; do \
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

.PHONY: app run run-headless-smoke visual-harness package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh run-ide-theme run-daw-theme tools tools-build graph graph-build test-space build-safety-check test test-region-validate-strict test-region-validate-contract test-runtime-source-policy test-archive-metrics-rollup test-coverage-metadata-contract metrics-rollup-gate test-shared-theme-font-adapter test-trace-contract test-worker-contract test-tile-loader-shutdown test-tile-source-archive test-route-service test-tile-presenter-policy test-presentation-stability test-polygon-cache-guardrails test-input-policy test-header-layer-layout test-window-resize test-tile-manager-residency test-phase-a-viewport-scenario test-phase-b-continuity-stress test-phase-d1-budget-control test-phase-d1-budget-matrix test-phase-d2-trace-matrix test-phase-d2-tuning-profiles test-phase-d2-trend-summary test-phase-d2-guardrails test-phase-d3-regression-gate test-phase-d3-contract-preview test-phase-d-throughput route route-rebuild region region-archive region-validate region-rebuild region-rebuild-archive tools-progress graph-progress region-progress region-progress-archive route-progress batch-regions disk-usage region-clean graph-clean prune-regions shared-check trace-latest vk-lib vk-check clean

-include $(DEPS)
