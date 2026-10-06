#ifndef MAPFORGE_TERRAIN_PACK_H
#define MAPFORGE_TERRAIN_PACK_H
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#define TERRAIN_PIXELS 258
#define TERRAIN_RGBA_BYTES (TERRAIN_PIXELS * TERRAIN_PIXELS * 4)
// Describes one fixed overview resolution and its tile grid.
typedef struct TerrainLevel {
    int width, height, cols, rows;
} TerrainLevel;
// Keeps georeferencing independent for each source raster.
typedef struct TerrainStyle {
    double extent[4];
    int mode;
    bool indexed;
    unsigned char palette[256][4];
    char area[64];
    TerrainLevel levels[5];
} TerrainStyle;
// Owns a validated read-only overview and its prepared bounded tile query.
typedef struct TerrainPack {
    sqlite3 *db;
    sqlite3_stmt *tile;
    TerrainStyle styles[10];
    int style_count;
    size_t bytes;
} TerrainPack;
// Opens a verified overview without changing an existing pack on failure.
bool terrain_pack_open(TerrainPack *pack, const char *path, char *error, size_t error_size);
// Releases the read-only pack and prepared statements.
void terrain_pack_close(TerrainPack *pack);
// Chooses the finest local raster fully covering the current view, with an overview fallback.
int terrain_pack_select(const TerrainPack *pack, int mode, double xmin, double ymin, double xmax,
                        double ymax);
// Decodes exactly one CRC-checked RGBA tile into a caller-owned fixed buffer.
bool terrain_pack_tile(TerrainPack *pack, int style, int lod, int col, int row, unsigned char *rgba);
#endif
