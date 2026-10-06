#ifndef MAPFORGE_REGION_PREPARE_H
#define MAPFORGE_REGION_PREPARE_H

#include "app/region_loader.h"
#include "camera/camera.h"
#include "map/tile_manager.h"
#include "core_workers.h"

// Reports synchronized CPU preparation progress without exposing unfinished caches.
typedef struct RegionPrepareProgress {
    bool done, ok, canceled;
    uint32_t examined, total, ready;
    uint64_t resident_bytes;
    char message[256];
} RegionPrepareProgress;

// Owns a single bounded worker and private decoded presentation until publication or disposal.
typedef struct RegionPrepare {
    CoreWorkers workers;
    pthread_t thread;
    CoreWorkerTask task;
    pthread_mutex_t mutex;
    bool cancel;
    RegionPrepareProgress progress;
    RegionInfo region;
    char name[128];
    RegionPackageValidationResult validation;
    Camera camera;
    int width, height;
    uint64_t cpu_limits[TILE_LAYER_COUNT];
    TileManager managers[TILE_LAYER_COUNT];
} RegionPrepare;

// Starts validation and bounded coarse decode without mutating the current map.
RegionPrepare *region_prepare_start(const RegionInfo *info, const Camera *camera, int width, int height,
                                    const uint64_t cpu_limits[TILE_LAYER_COUNT]);
// Requests cooperative cancellation; no join or tile work occurs on this call.
void region_prepare_cancel(RegionPrepare *prepare);
// Copies progress while keeping unfinished decoded tiles private to the worker.
RegionPrepareProgress region_prepare_progress(RegionPrepare *prepare);
// Joins the worker and frees only caches not transferred to the active map.
void region_prepare_destroy(RegionPrepare *prepare);

#endif
