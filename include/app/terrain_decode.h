#ifndef MAPFORGE_TERRAIN_DECODE_H
#define MAPFORGE_TERRAIN_DECODE_H
#include "map/terrain_pack.h"
#include <stdint.h>
#define TERRAIN_DECODE_CAPACITY 8
// Identifies one geographic raster tile within the current source generation.
typedef struct TerrainDecodeKey { int style, lod, col, row; } TerrainDecodeKey;
// Transfers one checked tile buffer from the decoder to the render owner.
typedef struct TerrainDecodeResult { TerrainDecodeKey key; unsigned char *rgba; bool ok; } TerrainDecodeResult;
// Reports bounded worker activity without exposing its read-only SQLite connection.
typedef struct TerrainDecodeStats { uint64_t submitted, decoded, stale, failed; size_t occupied; } TerrainDecodeStats;
// Owns one sleeping worker, a private pack connection and eight bounded results.
typedef struct TerrainDecode TerrainDecode;
// Starts an existing shared worker without renderer or personal-data access.
TerrainDecode *terrain_decode_create(void);
// Replaces source meaning and rejects every result from an earlier generation.
void terrain_decode_source(TerrainDecode *decoder, const char *path, const TerrainPack *metadata);
// Enqueues a tile without blocking on file IO or inflating the bounded queue.
bool terrain_decode_request(TerrainDecode *decoder, TerrainDecodeKey key);
// Takes one current-generation result; the caller owns and frees its RGBA buffer.
bool terrain_decode_take(TerrainDecode *decoder, TerrainDecodeResult *result);
// Returns synchronized counters and the number of occupied request/result slots.
TerrainDecodeStats terrain_decode_stats(TerrainDecode *decoder);
// Cancels queued work, joins the in-flight reader and closes its source before file removal.
void terrain_decode_destroy(TerrainDecode *decoder);
#endif
