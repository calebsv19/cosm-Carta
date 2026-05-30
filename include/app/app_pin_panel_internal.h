#ifndef MAPFORGE_APP_PIN_PANEL_INTERNAL_H
#define MAPFORGE_APP_PIN_PANEL_INTERNAL_H

#include "app/app_internal.h"

void app_pin_panel_layout(AppState *app);
void app_draw_pin_panel(AppState *app);
void app_draw_pins_overlay(AppState *app);
bool app_select_pin_at_screen_point(AppState *app, int screen_x, int screen_y);
bool app_pin_name_edit_insert_text(char *buffer, size_t cap, int *cursor, const char *text);
bool app_pin_name_edit_backspace(char *buffer, int *cursor);
bool app_pin_name_edit_move_left(const char *buffer, int *cursor);
bool app_pin_name_edit_move_right(const char *buffer, int *cursor);
void app_pin_panel_select_saved_pin(AppState *app, int index);
bool app_pin_panel_name_edit_active(const AppState *app);
void app_pin_panel_name_edit_deactivate(AppState *app);
void app_pin_panel_name_edit_sync_cursor_to_end(AppState *app);
void app_pin_panel_draw_name_field(AppState *app,
                                   const MapForgeThemePalette *palette,
                                   SDL_Color text,
                                   SDL_Color muted);
bool app_pin_panel_handle_name_click(AppState *app, int x, int y);
bool app_pin_panel_handle_name_runtime_inputs(AppState *app);
void app_pin_panel_draw_metadata(AppState *app);
bool app_pin_panel_handle_metadata_click(AppState *app, int x, int y);
bool app_pin_panel_row_has_route_start(const AppState *app, int pin_index);
bool app_pin_panel_row_has_route_goal(const AppState *app, int pin_index);
void app_pin_panel_clear_route_start(AppState *app);
void app_pin_panel_clear_route_goal(AppState *app);
void app_pin_panel_clear_route_bindings(AppState *app);
void app_pin_panel_cancel_list_drag(AppState *app);
bool app_pin_panel_handle_list_click(AppState *app, int x, int y);
bool app_pin_panel_handle_list_runtime_inputs(AppState *app);
bool app_pin_panel_handle_click(AppState *app, int x, int y);
bool app_pin_panel_handle_runtime_inputs(AppState *app);
bool app_pin_panel_handle_map_click(AppState *app, int x, int y);

#endif
