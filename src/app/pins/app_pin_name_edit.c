#include "app/app_internal.h"

#include <string.h>

static int app_pin_name_edit_clamp_cursor(const char *buffer, int cursor) {
    size_t len = 0u;
    if (!buffer) {
        return 0;
    }
    len = strnlen(buffer, MAPFORGE_PIN_NAME_CAPACITY);
    if (cursor < 0) {
        return 0;
    }
    if ((size_t)cursor > len) {
        return (int)len;
    }
    return cursor;
}

bool app_pin_name_edit_insert_text(char *buffer, size_t cap, int *cursor, const char *text) {
    size_t len = 0u;
    size_t add_len = 0u;
    int cursor_index = 0;
    if (!buffer || cap == 0u || !cursor || !text || text[0] == '\0') {
        return false;
    }
    len = strnlen(buffer, cap);
    add_len = strnlen(text, 31u);
    cursor_index = app_pin_name_edit_clamp_cursor(buffer, *cursor);
    if (len >= cap || add_len == 0u || len + add_len >= cap) {
        return false;
    }
    memmove(buffer + cursor_index + (int)add_len,
            buffer + cursor_index,
            len - (size_t)cursor_index + 1u);
    memcpy(buffer + cursor_index, text, add_len);
    *cursor = cursor_index + (int)add_len;
    return true;
}

bool app_pin_name_edit_backspace(char *buffer, int *cursor) {
    size_t len = 0u;
    int cursor_index = 0;
    if (!buffer || !cursor) {
        return false;
    }
    len = strnlen(buffer, MAPFORGE_PIN_NAME_CAPACITY);
    cursor_index = app_pin_name_edit_clamp_cursor(buffer, *cursor);
    if (cursor_index <= 0 || len == 0u) {
        return false;
    }
    memmove(buffer + cursor_index - 1,
            buffer + cursor_index,
            len - (size_t)cursor_index + 1u);
    *cursor = cursor_index - 1;
    return true;
}

bool app_pin_name_edit_move_left(const char *buffer, int *cursor) {
    int cursor_index = 0;
    if (!buffer || !cursor) {
        return false;
    }
    cursor_index = app_pin_name_edit_clamp_cursor(buffer, *cursor);
    if (cursor_index <= 0) {
        *cursor = 0;
        return false;
    }
    *cursor = cursor_index - 1;
    return true;
}

bool app_pin_name_edit_move_right(const char *buffer, int *cursor) {
    size_t len = 0u;
    int cursor_index = 0;
    if (!buffer || !cursor) {
        return false;
    }
    len = strnlen(buffer, MAPFORGE_PIN_NAME_CAPACITY);
    cursor_index = app_pin_name_edit_clamp_cursor(buffer, *cursor);
    if ((size_t)cursor_index >= len) {
        *cursor = (int)len;
        return false;
    }
    *cursor = cursor_index + 1;
    return true;
}
