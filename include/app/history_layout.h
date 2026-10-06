#ifndef MAPFORGE_HISTORY_LAYOUT_H
#define MAPFORGE_HISTORY_LAYOUT_H
#include <SDL.h>
#include <stdbool.h>
// Shares one responsive geometry contract between history drawing, pointer input and diagnostics.
typedef struct HistoryLayout {
    SDL_FRect actions[29], legend, list, detail;
    int rows;
} HistoryLayout;
// Computes bounded primary controls, optional tools, route rows and the selected-route card.
HistoryLayout history_layout(SDL_FRect pane, bool tools, bool inspect);
#endif
