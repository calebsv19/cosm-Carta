#ifndef CARTA_UI_ROLLOUT_H
#define CARTA_UI_ROLLOUT_H
#include "ui/text_input.h"
#include "app/history_views.h"
#include "kit_pane.h"
#include "kit_pane_composition.h"
#include "kit_ui_window_sdl.h"
#include "kit_ui_window_probe_sdl.h"
struct AppState;
// Transient shared UI contract owners; never serialized with product records.
typedef struct CartaUiRollout {
    CartaTextInput pin_name, ingest_root;
    KitPaneComposition panes;
    KitPanePointerOwner pointer;
    KitPaneSplitterInteraction splitter;
    CorePaneNode nodes[3];
    float sidebar_ratio;
    bool pane_initialized;
    KitUiWindowState window;
    KitUiWindowProbe probe;
    uint64_t presented_frames;
    bool window_initialized;
    HistoryViewSelection history_bindings[3];
    char pin_target[64];
    uint64_t store_signature,route_bindings;
} CartaUiRollout;
// Routes the last two focused text hosts above background shortcuts.
bool carta_text_hosts_event(struct AppState *,const SDL_Event *);
// Resolves shared sidebar/map geometry and observes subtle splitter interaction.
void carta_panes_layout(struct AppState *);
bool carta_panes_event(struct AppState *,const SDL_Event *);
void carta_panes_draw(struct AppState *);
// Observes native window changes and cancels transient interactions.
bool carta_window_event(struct AppState *,const SDL_Event *);
bool carta_window_refresh(struct AppState *);
int carta_window_probe_tick(struct AppState *);
#endif
