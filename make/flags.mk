SDL_CFLAGS :=
SDL_LIBS :=
SDL_TTF_CFLAGS :=
SDL_TTF_LIBS :=
VULKAN_CFLAGS :=
VULKAN_LIBS :=
VULKAN_VALIDATION_DYLD_PATH :=
JSON_CFLAGS :=
JSON_LIBS :=
SQLITE_CFLAGS :=
SQLITE_LIBS :=

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

ifeq ($(UNAME_S),Darwin)
VULKAN_VALIDATION_DYLD_PATH := $(TARGET_HOMEBREW_PREFIX)/lib
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
APP_CFLAGS += -I$(CORE_HEADLESS_JOB_DIR)/include
APP_CFLAGS += -I$(CORE_THEME_DIR)/include
APP_CFLAGS += -I$(CORE_FONT_DIR)/include
APP_CFLAGS += -I$(CORE_PANE_DIR)/include
APP_CFLAGS += -I$(CORE_VIEWPORT2D_DIR)/include
APP_CFLAGS += -I$(KIT_RUNTIME_DIAG_DIR)/include
APP_CFLAGS += -I$(KIT_RENDER_DIR)/include
APP_CFLAGS += -I$(KIT_WORKSPACE_AUTHORING_DIR)/include
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
HOST_CFLAGS += -I$(CORE_HEADLESS_JOB_DIR)/include
HOST_CFLAGS += -I$(CORE_THEME_DIR)/include
HOST_CFLAGS += -I$(CORE_FONT_DIR)/include
HOST_CFLAGS += -I$(CORE_PANE_DIR)/include
HOST_CFLAGS += -I$(CORE_VIEWPORT2D_DIR)/include
HOST_CFLAGS += -I$(KIT_RUNTIME_DIAG_DIR)/include
HOST_CFLAGS += -I$(KIT_RENDER_DIR)/include
HOST_CFLAGS += -I$(KIT_WORKSPACE_AUTHORING_DIR)/include
