#ifndef MAPFORGE_MAP_POLYGON_TRIANGULATOR_H
#define MAPFORGE_MAP_POLYGON_TRIANGULATOR_H

#include <stdbool.h>
#include <stdint.h>

// Selects which triangulation mode to use for polygon fills.
typedef enum PolygonTriangulationMode {
    POLYGON_TRIANGULATION_EAR_CLIP = 0,
    POLYGON_TRIANGULATION_FAN = 1
} PolygonTriangulationMode;

// Allows a background caller to cancel expensive exact triangulation cooperatively.
typedef bool (*PolygonTriangulationCancelFn)(void *context);
// Triangulates with bounded cancellation checks; never returns partial geometry on cancellation.
bool polygon_triangulate_cancelable(const uint16_t *points, uint32_t count,
    PolygonTriangulationMode mode, int *out_indices, int *out_index_count, int max_indices,
    PolygonTriangulationCancelFn canceled, void *context);

// Triangulates a simple polygon ring (no holes). Returns false on failure.
bool polygon_triangulate(const uint16_t *points,
                         uint32_t count,
                         PolygonTriangulationMode mode,
                         int *out_indices,
                         int *out_index_count,
                         int max_indices);

#endif
