#ifndef MAPFORGE_APP_UI_INTERNAL_H
#define MAPFORGE_APP_UI_INTERNAL_H

#include "app/app_internal.h"

MapForgeThemePalette app_ui_theme_palette(void);
float app_header_layer_strip_start_x(float strip_x, float strip_w, float content_w, float scroll_px);

typedef enum AppUiTextRole {
    APP_UI_TEXT_ROLE_PANEL_TITLE = 0,
    APP_UI_TEXT_ROLE_CONTROL,
    APP_UI_TEXT_ROLE_PANEL_BODY,
    APP_UI_TEXT_ROLE_DIAGNOSTIC
} AppUiTextRole;

float app_ui_text_scale(AppUiTextRole role);
int app_ui_text_line_height(AppUiTextRole role);
int app_ui_text_width(const char *text, AppUiTextRole role);
bool app_handle_hud_clicks(AppState *app);

#endif
