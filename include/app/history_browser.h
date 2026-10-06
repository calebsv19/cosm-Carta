#ifndef MAPFORGE_HISTORY_BROWSER_H
#define MAPFORGE_HISTORY_BROWSER_H
#include "app/app_internal.h"
#define HISTORY_ROW_HEIGHT 38.0f
// Owns an multi-view chooser without touching personal records.
typedef struct HistoryBrowser { bool open; int focus; float offset;
    KitUiSurface controls; unsigned scope; char identities[16][140]; } HistoryBrowser;
// Computes a finite pixel offset using the existing shared scroll evaluator.
float history_browser_scroll(SDL_FRect viewport, int total, float row_height, float offset, float wheel);
// Reads fractional SDL scrolling with discrete-wheel fallback and flipped direction.
float history_browser_wheel(const SDL_Event *event);
// Maps a clipped pointer position to its content row.
int history_browser_row(SDL_FRect viewport, float offset, float row_height, int x, int y);
// Returns a bounded proportional scroll thumb, empty when content fits.
SDL_FRect history_browser_thumb(SDL_FRect viewport, int total, float row_height, float offset);
// Labels the selected progress/source/collection groups.
void history_browser_view_label(AppState *app, char *label, size_t cap);
// Opens the chooser focused on the current view.
void history_browser_open(HistoryBrowser *browser, AppState *app);
// Handles modal selection, returning true when group preferences change.
bool history_browser_event(HistoryBrowser *browser, AppState *app, const SDL_Event *event);
// Draws the view chooser over the current map and pane.
void history_browser_draw(HistoryBrowser *browser, AppState *app);
#endif
