#ifndef MAPFORGE_APP_PERSIST_STATE_H
#define MAPFORGE_APP_PERSIST_STATE_H

#include "app/app_internal.h"

void app_load_persisted_view_state(AppState *app);
void app_save_persisted_view_state(const AppState *app);

#endif
