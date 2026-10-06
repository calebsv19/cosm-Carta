#ifndef MAPFORGE_TERRAIN_VECTORS_H
#define MAPFORGE_TERRAIN_VECTORS_H
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#define TERRAIN_VECTOR_BYTES (512u * 1024u)
#define TERRAIN_VECTOR_LABELS 2048
// Describes one DEM-derived area with three geographic contour levels.
typedef struct TerrainVectorArea {
    double extent[4], unit[3];
    int interval[3];
} TerrainVectorArea;
// Stores a short official basemap name independent of personal visit records.
typedef struct TerrainVectorLabel {
    double x, y;
    char name[121], kind[51];
} TerrainVectorLabel;
// Holds one interpolated contour segment in area-relative projected meters.
typedef struct TerrainVectorSegment { float x0, y0, x1, y1, elevation; } TerrainVectorSegment;
// Owns a validated read-only vector sidecar, bounded names and a prepared tile lookup.
typedef struct TerrainVectors {
    sqlite3 *db;
    sqlite3_stmt *tile;
    TerrainVectorArea areas[5];
    int count;
    TerrainVectorLabel *labels;
    size_t label_count, bytes;
} TerrainVectors;
// Opens a sidecar atomically while preserving an existing reader if validation fails.
bool terrain_vectors_open(TerrainVectors *v, const char *path, char *error, size_t error_size);
// Releases sidecar statements and names.
void terrain_vectors_close(TerrainVectors *v);
// Loads one fixed-budget CRC-checked tile; missing tiles are valid empty geographic cells.
bool terrain_vectors_tile(TerrainVectors *v, int area, int lod, int col, int row,
                          TerrainVectorSegment **segments, size_t *count);
// Selects the finest area fully containing the view or the overview fallback.
int terrain_vectors_select(const TerrainVectors *v, double xmin, double ymin, double xmax, double ymax);
#endif
