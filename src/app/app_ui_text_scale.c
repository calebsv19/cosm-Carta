#include "app/app_ui_internal.h"

#include "ui/font.h"

float app_ui_text_scale(AppUiTextRole role) {
    switch (role) {
        case APP_UI_TEXT_ROLE_PANEL_TITLE:
        case APP_UI_TEXT_ROLE_CONTROL:
            return 0.95f;
        case APP_UI_TEXT_ROLE_PANEL_BODY:
        case APP_UI_TEXT_ROLE_DIAGNOSTIC:
            return 1.0f;
        default:
            return 1.0f;
    }
}

int app_ui_text_line_height(AppUiTextRole role) {
    return ui_font_line_height(app_ui_text_scale(role));
}

int app_ui_text_width(const char *text, AppUiTextRole role) {
    return ui_measure_text_width(text, app_ui_text_scale(role));
}
