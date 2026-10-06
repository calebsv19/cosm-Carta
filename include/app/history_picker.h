#ifndef CARTA_HISTORY_PICKER_H
#define CARTA_HISTORY_PICKER_H
#include "ui/modal_controls.h"
typedef struct HistoryPickerControls {
    KitUiSurface surface; unsigned scope; int offset;
    char query[128],names[6][128];
} HistoryPickerControls;
// UI intent only: 0..5 choose, 6 previous, 7 next; product host owns the action.
int history_picker_event(HistoryPickerControls *,struct AppState *,const char *,int,SDL_FRect,const SDL_Event *);
void history_picker_draw(HistoryPickerControls *,struct AppState *,const char *,int,SDL_FRect);
void history_picker_cancel(HistoryPickerControls *);
#endif
