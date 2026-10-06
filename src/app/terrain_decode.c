#include "app/terrain_decode.h"
#include "core_workers.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// Keeps request storage alive until its queued callback has returned or been canceled.
typedef struct DecodeJob {
    struct TerrainDecode *owner;
    uint64_t generation;
    TerrainDecodeKey key;
    unsigned char *rgba;
    int state; /* 0 free, 1 queued/running, 2 ready */
    bool ok;
} DecodeJob;
// Separates render-owned admission metadata from the worker-owned SQLite connection.
struct TerrainDecode {
    CoreWorkers workers;
    pthread_t thread;
    CoreWorkerTask tasks[TERRAIN_DECODE_CAPACITY];
    pthread_mutex_t mutex;
    DecodeJob jobs[TERRAIN_DECODE_CAPACITY];
    uint64_t generation, reader_generation;
    char path[4096];
    TerrainPack metadata, reader;
    TerrainDecodeStats stats;
};
// Decodes privately and publishes only if the requested source is still current.
static void *decode(void *context) {
    DecodeJob *job = context;
    TerrainDecode *d = job->owner;
    char path[4096];
    TerrainPack expected;
    pthread_mutex_lock(&d->mutex);
    uint64_t generation = job->generation;
    if (generation != d->generation) {
        job->state = 0; d->stats.stale++;
        pthread_mutex_unlock(&d->mutex); return NULL;
    }
    snprintf(path, sizeof(path), "%s", d->path);
    expected = d->metadata;
    TerrainDecodeKey key = job->key;
    pthread_mutex_unlock(&d->mutex);
    if (d->reader_generation != generation) {
        terrain_pack_close(&d->reader);
        d->reader_generation = generation;
        char error[160];
        bool ok = terrain_pack_open(&d->reader, path, error, sizeof(error));
        if (ok && (d->reader.bytes != expected.bytes || d->reader.style_count != expected.style_count ||
                   memcmp(d->reader.styles, expected.styles, sizeof(expected.styles))))
            terrain_pack_close(&d->reader);
    }
    unsigned char *rgba = d->reader.db ? malloc(TERRAIN_RGBA_BYTES) : NULL;
    bool ok = rgba && terrain_pack_tile(&d->reader, key.style, key.lod, key.col, key.row, rgba);
    if (!ok) { free(rgba); rgba = NULL; }
    pthread_mutex_lock(&d->mutex);
    d->stats.decoded++;
    if (generation != d->generation) {
        free(rgba); job->state = 0; d->stats.stale++;
    } else {
        job->rgba = rgba; job->ok = ok; job->state = 2;
        if (!ok) d->stats.failed++;
    }
    pthread_mutex_unlock(&d->mutex);
    return NULL;
}
TerrainDecode *terrain_decode_create(void) {
    TerrainDecode *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    if (pthread_mutex_init(&d->mutex, NULL)) { free(d); return NULL; }
    if (!core_workers_init(&d->workers, &d->thread, 1, d->tasks, TERRAIN_DECODE_CAPACITY, NULL)) {
        pthread_mutex_destroy(&d->mutex); free(d); return NULL;
    }
    for (int i = 0; i < TERRAIN_DECODE_CAPACITY; i++) d->jobs[i].owner = d;
    return d;
}
void terrain_decode_source(TerrainDecode *d, const char *path, const TerrainPack *metadata) {
    if (!d) return;
    pthread_mutex_lock(&d->mutex);
    d->generation++;
    snprintf(d->path, sizeof(d->path), "%s", path ? path : "");
    d->metadata = metadata ? *metadata : (TerrainPack){0};
    d->metadata.db = NULL; d->metadata.tile = NULL;
    for (int i = 0; i < TERRAIN_DECODE_CAPACITY; i++) if (d->jobs[i].state == 2) {
        free(d->jobs[i].rgba); d->jobs[i].rgba = NULL; d->jobs[i].state = 0; d->stats.stale++;
    }
    pthread_mutex_unlock(&d->mutex);
}
bool terrain_decode_request(TerrainDecode *d, TerrainDecodeKey key) {
    if (!d) return false;
    pthread_mutex_lock(&d->mutex);
    DecodeJob *free_job = NULL;
    for (int i = 0; i < TERRAIN_DECODE_CAPACITY; i++) {
        DecodeJob *job = &d->jobs[i];
        if (!job->state) free_job = job;
        else if (job->generation == d->generation && job->key.style == key.style &&
                 job->key.lod == key.lod && job->key.col == key.col && job->key.row == key.row) {
            pthread_mutex_unlock(&d->mutex); return true;
        }
    }
    bool ok = free_job && *d->path;
    if (ok) {
        free_job->key = key; free_job->generation = d->generation; free_job->state = 1;
        ok = core_workers_submit(&d->workers, decode, free_job);
        if (ok) d->stats.submitted++; else free_job->state = 0;
    }
    pthread_mutex_unlock(&d->mutex);
    return ok;
}
bool terrain_decode_take(TerrainDecode *d, TerrainDecodeResult *result) {
    if (!d || !result) return false;
    pthread_mutex_lock(&d->mutex);
    bool found = false;
    for (int i = 0; i < TERRAIN_DECODE_CAPACITY; i++) {
        DecodeJob *job = &d->jobs[i];
        if (job->state == 2 && job->generation == d->generation) {
            *result = (TerrainDecodeResult){job->key, job->rgba, job->ok};
            job->rgba = NULL; job->state = 0; found = true; break;
        }
    }
    pthread_mutex_unlock(&d->mutex);
    return found;
}
TerrainDecodeStats terrain_decode_stats(TerrainDecode *d) {
    TerrainDecodeStats stats = {0};
    if (!d) return stats;
    pthread_mutex_lock(&d->mutex);
    stats = d->stats;
    for (int i = 0; i < TERRAIN_DECODE_CAPACITY; i++) stats.occupied += d->jobs[i].state != 0;
    pthread_mutex_unlock(&d->mutex);
    return stats;
}
void terrain_decode_destroy(TerrainDecode *d) {
    if (!d) return;
    terrain_decode_source(d, NULL, NULL);
    core_workers_shutdown_with_mode(&d->workers, CORE_WORKERS_SHUTDOWN_CANCEL);
    terrain_pack_close(&d->reader);
    for (int i = 0; i < TERRAIN_DECODE_CAPACITY; i++) free(d->jobs[i].rgba);
    pthread_mutex_destroy(&d->mutex); free(d);
}
