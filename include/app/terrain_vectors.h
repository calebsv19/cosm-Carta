#ifndef MAPFORGE_APP_TERRAIN_VECTORS_H
#define MAPFORGE_APP_TERRAIN_VECTORS_H
struct AppState;
// Reports the last frame's submitted contours for native presentation diagnostics.
unsigned terrain_vectors_drawn_lines(void);
// Prepares and draws bounded world-projected contour segments beneath history geometry.
void terrain_vectors_draw(struct AppState *app);
// Places independent fixed-size official names and elevation labels without collisions.
void terrain_vectors_labels_draw(struct AppState *app);
// Releases process-owned vector data at shutdown or explicit terrain reload.
void terrain_vectors_shutdown(void);
#include <stddef.h>
#include <stdbool.h>
// Finds a bounded official peak by name in the installed read-only terrain sidecar.
bool terrain_vectors_peak_query(const char *query, size_t ordinal, char *name, size_t cap,
                                double *lat, double *lon);
// Reports whether coordinates intersect the installed terrain extent without loading another map.
bool terrain_vectors_covers(double min_lat,double min_lon,double max_lat,double max_lon);
#endif
