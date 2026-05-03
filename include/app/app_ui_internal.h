#ifndef MAPFORGE_APP_UI_INTERNAL_H
#define MAPFORGE_APP_UI_INTERNAL_H

#include "ui/shared_theme_font_adapter.h"

MapForgeThemePalette app_ui_theme_palette(void);
float app_header_layer_strip_start_x(float strip_x, float strip_w, float content_w, float scroll_px);

#endif
