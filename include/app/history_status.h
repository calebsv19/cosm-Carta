#ifndef MAPFORGE_HISTORY_STATUS_H
#define MAPFORGE_HISTORY_STATUS_H
#include "app/history.h"
// Reads independent personal progress without treating geometry as a completion assertion.
const char *history_progress(struct json_object *root, struct json_object *record);
// Tests personal peak tracking separately from neutral geographic context.
bool history_peak_tracked(struct json_object *root, struct json_object *record);
// Validates the bounded personal progress vocabulary.
bool history_progress_valid(const char *value);
// Executes guarded conservative migration and explicit named-collection reconciliation.
struct json_object *history_collections_command(const char *path, MapForgePinsFile *file,
                                                struct json_object *request);
#endif
