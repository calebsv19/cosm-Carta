CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../../core/core_base
CORE_PANE_DIR ?= ../../core/core_pane
CORE_THEME_DIR ?= ../../core/core_theme
CORE_FONT_DIR ?= ../../core/core_font
KIT_RENDER_DIR ?= ../kit_render

INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(KIT_RENDER_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libkit_workspace_authoring.a
LIB_OBJ = $(OBJ_DIR)/kit_workspace_authoring.o $(OBJ_DIR)/kit_workspace_authoring_ui_overlay.o $(OBJ_DIR)/kit_workspace_authoring_ui_hud.o $(OBJ_DIR)/kit_workspace_authoring_ui_render.o $(OBJ_DIR)/kit_workspace_authoring_ui_submit.o $(OBJ_DIR)/kit_workspace_authoring_ui_font_theme.o
TEST_BIN = $(OBJ_DIR)/kit_workspace_authoring_test
CORE_BASE_LIB = $(CORE_BASE_DIR)/build/libcore_base.a
CORE_THEME_LIB = $(CORE_THEME_DIR)/build/libcore_theme.a
CORE_FONT_LIB = $(CORE_FONT_DIR)/build/libcore_font.a
KIT_RENDER_LIB = $(KIT_RENDER_DIR)/build/default/libkit_render.a

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/kit_workspace_authoring.o: src/kit_workspace_authoring.c include/kit_workspace_authoring.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_workspace_authoring_ui_overlay.o: src/ui/kit_workspace_authoring_ui_overlay.c include/kit_workspace_authoring_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_workspace_authoring_ui_hud.o: src/ui/kit_workspace_authoring_ui_hud.c include/kit_workspace_authoring_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_workspace_authoring_ui_render.o: src/ui/kit_workspace_authoring_ui_render.c include/kit_workspace_authoring_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_workspace_authoring_ui_submit.o: src/ui/kit_workspace_authoring_ui_submit.c include/kit_workspace_authoring_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/kit_workspace_authoring_ui_font_theme.o: src/ui/kit_workspace_authoring_ui_font_theme.c include/kit_workspace_authoring_ui.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(CORE_BASE_LIB):
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_THEME_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_THEME_DIR)

$(CORE_FONT_LIB): $(CORE_BASE_LIB)
	$(MAKE) -C $(CORE_FONT_DIR)

$(KIT_RENDER_LIB): $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	$(MAKE) -C $(KIT_RENDER_DIR) KIT_RENDER_ENABLE_VK=0

$(TEST_BIN): tests/kit_workspace_authoring_test.c $(LIB) $(KIT_RENDER_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(KIT_RENDER_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)
