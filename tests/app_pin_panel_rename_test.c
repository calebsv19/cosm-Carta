#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_insert_backspace_and_cursor_moves(void) {
    char buffer[64];
    int cursor = 0;

    snprintf(buffer, sizeof(buffer), "Untitled 0");
    cursor = (int)strlen(buffer);
    assert(app_pin_name_edit_move_left(buffer, &cursor));
    assert(cursor == 9);
    assert(app_pin_name_edit_move_left(buffer, &cursor));
    assert(cursor == 8);
    assert(app_pin_name_edit_insert_text(buffer, sizeof(buffer), &cursor, "X"));
    assert(strcmp(buffer, "UntitledX 0") == 0);
    assert(cursor == 9);
    assert(app_pin_name_edit_backspace(buffer, &cursor));
    assert(strcmp(buffer, "Untitled 0") == 0);
    assert(cursor == 8);
}

static void test_cursor_clamps_at_bounds(void) {
    char buffer[64];
    int cursor = 0;

    snprintf(buffer, sizeof(buffer), "AB");
    cursor = 0;
    assert(!app_pin_name_edit_move_left(buffer, &cursor));
    assert(cursor == 0);
    cursor = 2;
    assert(!app_pin_name_edit_move_right(buffer, &cursor));
    assert(cursor == 2);
}

int main(void) {
    test_insert_backspace_and_cursor_moves();
    test_cursor_clamps_at_bounds();
    printf("app_pin_panel_rename_test: success\n");
    return 0;
}
