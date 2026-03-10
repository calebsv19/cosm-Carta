CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../../core/core_base
CORE_THEME_DIR ?= ../../core/core_theme
CORE_FONT_DIR ?= ../../core/core_font
KIT_RENDER_DIR ?= ../kit_render
KIT_RENDER_ENABLE_VK ?= 0
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/default
ifeq ($(KIT_RENDER_ENABLE_VK),1)
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/vk
endif

INC = -Iinclude -I$(KIT_RENDER_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libkit_pane.a
LIB_OBJ = $(OBJ_DIR)/kit_pane.o
TEST_BIN = $(OBJ_DIR)/kit_pane_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(KIT_RENDER_BUILD_DIR)/libkit_render.a:
	$(MAKE) -C $(KIT_RENDER_DIR) KIT_RENDER_ENABLE_VK=$(KIT_RENDER_ENABLE_VK)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_THEME_DIR)/build/libcore_theme.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_THEME_DIR)

$(CORE_FONT_DIR)/build/libcore_font.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_FONT_DIR)

$(LIB_OBJ): src/kit_pane.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/kit_pane_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
