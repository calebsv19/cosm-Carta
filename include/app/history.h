#ifndef MAPFORGE_HISTORY_H
#define MAPFORGE_HISTORY_H
#include "app/place_repository.h"
#include <stddef.h>
// Preserves one original GPX point and optional source fields without road snapping.
typedef struct HistoryPoint {
    double lat, lon, elevation, time_s, distance_m;
    unsigned segment;
} HistoryPoint;
// Owns a bounded original source and normalized in-memory geometry.
typedef struct HistoryTrack {
    unsigned char *bytes;
    size_t byte_count, point_count, segment_count, track_count, route_point_count, waypoint_count;
    HistoryPoint *points;
    char hash[65], name[256];
    double min_lat, max_lat, min_lon, max_lon, distance_m;
    size_t elevation_count, time_count, invalid_optional_count, time_reversals, large_steps;
} HistoryTrack;
// Releases an original source and all normalized points.
void history_track_free(HistoryTrack *track);
// Validates bounded UTF-8 GPX 1.1 bytes and normalizes independent track segments.
bool history_parse(const void *bytes, size_t size, HistoryTrack *out, char *error, size_t cap);
// Reads a bounded regular source without following a symlink.
bool history_read(const char *path, HistoryTrack *out, char *error, size_t cap);
// Retrieves and verifies one immutable original from the personal repository.
bool history_asset_load(const char *path, const char *hash, HistoryTrack *out, char *error, size_t cap);
// Executes guarded preview/import/manual-history/list/export/archive operations.
struct json_object *history_command(const char *path, MapForgePinsFile *file, struct json_object *request);
// Saves canonical metadata and one optional immutable source in a single transaction.
bool map_forge_places_save_asset(const char *path, struct json_object *root, uint64_t expected,
                                 const HistoryTrack *asset, const char *restore_source, char *error,
                                 size_t cap);
#endif
