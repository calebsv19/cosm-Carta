#ifndef MAPFORGE_HISTORY_VIEWS_H
#define MAPFORGE_HISTORY_VIEWS_H
#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <json-c/json.h>
#define HISTORY_VIEWS_MAX 32
#define HISTORY_VIEW_COLORS 12
struct AppState;
// Stores one selected preset or stable saved collection and its display preferences.
typedef struct HistoryViewSelection {
    int filter; /* -1 is a saved collection, 0..6 are personal/source presets. */
    char collection[129];
    bool map_visible;
    uint8_t color;
} HistoryViewSelection;
// Owns a bounded union of groups; initialized empty deliberately means no groups.
typedef struct HistoryViewSet {
    bool initialized;
    size_t count;
    HistoryViewSelection items[HISTORY_VIEWS_MAX];
} HistoryViewSet;
// Migrates an older single-view preference lazily without changing personal records.
void history_views_ensure(struct AppState *app);
// Selects just one view for legacy program actions and explicit Solo controls.
void history_views_single(struct AppState *app, int filter, const char *collection);
// Counts available presets and current named saved collections.
int history_views_choice_count(struct AppState *app);
// Resolves one available choice into a stable group identity and default color.
HistoryViewSelection history_views_choice(struct AppState *app, int choice);
// Finds a selected group by its stable identity, independent of source list ordering.
int history_views_find(struct AppState *app, HistoryViewSelection choice);
// Adds or removes a selected group, preserving all other groups and settings.
bool history_views_toggle(struct AppState *app, HistoryViewSelection choice);
// Labels a selected group without asserting a completion status.
void history_views_name(struct AppState *app, HistoryViewSelection choice, char *name, size_t cap);
// Returns the opaque display color for a selected group or chooser candidate.
SDL_Color history_views_color(HistoryViewSelection choice);
// Blends an opaque surface with a group tint using a bounded amount.
SDL_Color history_views_tint(SDL_Color base, SDL_Color color, float amount);
// Returns selected group membership, including explicit paired-activity inheritance.
uint32_t history_views_membership(struct AppState *app, struct json_object *record, bool map_only);
// Selects the first matching visible group color, falling back to the first selected match.
SDL_Color history_views_record_color(struct AppState *app, struct json_object *record);
// Reads validated UI-only selection preferences with legacy migration and explicit empty support.
void history_views_load(struct AppState *app, struct json_object *root);
// Writes stable group identities, visibility and colors to UI preferences only.
void history_views_save(const struct AppState *app, struct json_object *root);
#endif
