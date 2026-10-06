CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../../core/core_base
CORE_LAYOUT_DIR ?= ../../core/core_layout
CORE_PANE_DIR ?= ../../core/core_pane
CORE_THEME_DIR ?= ../../core/core_theme
CORE_FONT_DIR ?= ../../core/core_font
KIT_RENDER_DIR ?= ../kit_render
KIT_RENDER_ENABLE_VK ?= 0
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/default
ifeq ($(KIT_RENDER_ENABLE_VK),1)
KIT_RENDER_BUILD_DIR = $(KIT_RENDER_DIR)/build/vk
endif

INC = -I$(CORE_LAYOUT_DIR)/include -Iinclude -I$(KIT_RENDER_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libkit_pane.a
LIB_OBJ = $(OBJ_DIR)/kit_pane.o $(OBJ_DIR)/kit_pane_composition.o $(OBJ_DIR)/kit_pane_host.o $(OBJ_DIR)/kit_pane_header.o
TEST_BIN = $(OBJ_DIR)/kit_pane_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(KIT_RENDER_BUILD_DIR)/libkit_render.a:
	$(MAKE) -C $(KIT_RENDER_DIR) KIT_RENDER_ENABLE_VK=$(KIT_RENDER_ENABLE_VK)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_PANE_DIR)/build/libcore_pane.a:
	$(MAKE) -C $(CORE_PANE_DIR)

$(CORE_THEME_DIR)/build/libcore_theme.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_THEME_DIR)

$(CORE_FONT_DIR)/build/libcore_font.a: $(CORE_BASE_DIR)/build/libcore_base.a
	$(MAKE) -C $(CORE_FONT_DIR)

$(OBJ_DIR)/kit_pane.o: src/kit_pane.c include/kit_pane.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/kit_pane_test.c $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(CORE_PANE_DIR)/build/libcore_pane.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(KIT_RENDER_BUILD_DIR)/libkit_render.a $(CORE_PANE_DIR)/build/libcore_pane.a $(CORE_THEME_DIR)/build/libcore_theme.a $(CORE_FONT_DIR)/build/libcore_font.a $(CORE_BASE_DIR)/build/libcore_base.a -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)

$(OBJ_DIR)/kit_pane_composition.o: src/kit_pane_composition.c include/kit_pane_composition.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

.PHONY: test-composition
test: test-composition
test-composition: $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) tests/kit_pane_composition_test.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $(OBJ_DIR)/kit_pane_composition_test
	./$(OBJ_DIR)/kit_pane_composition_test

.PHONY: test-composition-sdl
test-composition-sdl: $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) $(shell pkg-config --cflags sdl2) tests/kit_pane_composition_sdl_test.c src/kit_pane_composition_sdl.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a $(shell pkg-config --libs sdl2) -lm -o $(OBJ_DIR)/kit_pane_composition_sdl_test
	./$(OBJ_DIR)/kit_pane_composition_sdl_test

$(OBJ_DIR)/kit_pane_host.o: src/kit_pane_host.c include/kit_pane_host.h include/kit_pane_composition.h $(CORE_LAYOUT_DIR)/include/core_layout.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
$(OBJ_DIR)/kit_pane_header.o: src/kit_pane_header.c include/kit_pane_host.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
$(CORE_LAYOUT_DIR)/build/libcore_layout.a:
	$(MAKE) -C $(CORE_LAYOUT_DIR)
.PHONY: test-host
test: test-host
test-host: $(LIB) $(CORE_LAYOUT_DIR)/build/libcore_layout.a $(CORE_BASE_DIR)/build/libcore_base.a
	$(CC) $(CFLAGS) $(INC) tests/kit_pane_host_test.c $(LIB) $(CORE_LAYOUT_DIR)/build/libcore_layout.a $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $(OBJ_DIR)/kit_pane_host_test
	./$(OBJ_DIR)/kit_pane_host_test
