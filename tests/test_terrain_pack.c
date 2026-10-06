#include "map/terrain_pack.h"
#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Verifies real pack admission/CRC and fail-closed replacement without writing the source.
int main(int argc, char **argv) {
    assert(argc == 3);
    TerrainPack p = {0};
    char err[160];
    unsigned char *rgba = malloc(TERRAIN_RGBA_BYTES);
    assert(rgba);
    assert(terrain_pack_open(&p, argv[1], err, sizeof(err)));
    sqlite3 *original = p.db;
    assert(!terrain_pack_open(&p, "/private/tmp/nonexistent-s8-terrain", err, sizeof(err)));
    assert(p.db == original);
    int n = 0;
    for (int s = 0; s < 2; s++)
        for (int l = 0; l < 5; l++)
            for (int y = 0; y < p.styles[s].levels[l].rows; y++)
                for (int x = 0; x < p.styles[s].levels[l].cols; x++) {
                    assert(terrain_pack_tile(&p, s, l, x, y, rgba));
                    n++;
                }
    assert(n == 10);
    // Local detail selection covers the whole rotated viewport and keeps an overview fallback.
    p.style_count = 4;
    p.styles[2] = p.styles[0];
    p.styles[3] = p.styles[1];
    for (int i = 2; i < 4; i++) {
        p.styles[i].extent[0] = p.styles[i].extent[1] = -500;
        p.styles[i].extent[2] = p.styles[i].extent[3] = 500;
        p.styles[i].levels[0].width = 512;
    }
    assert(terrain_pack_select(&p, 0, -100, -100, 100, 100) == 2);
    assert(terrain_pack_select(&p, 1, -100, -100, 100, 100) == 3);
    assert(terrain_pack_select(&p, 1, -600, -100, 100, 100) == 1);
    p.style_count = 2;
    assert(!terrain_pack_tile(&p, 1, 5, 0, 0, rgba));
    assert(!terrain_pack_tile(&p, 1, 0, -1, 0, rgba));
    // A bad CRS replacement cannot close the currently usable reader.
    sqlite3 *db = NULL;
    assert(sqlite3_open(argv[2], &db) == SQLITE_OK);
    assert(sqlite3_exec(db, "update metadata set value=replace(value,'EPSG:3857','EPSG:4326')", 0, 0, 0) ==
           SQLITE_OK);
    sqlite3_close(db);
    assert(!terrain_pack_open(&p, argv[2], err, sizeof(err)));
    assert(p.db == original);
    assert(terrain_pack_tile(&p, 0, 0, 0, 0, rgba));
    assert(sqlite3_open(argv[2], &db) == SQLITE_OK);
    sqlite3_exec(db, "update metadata set value=replace(value,'EPSG:4326','EPSG:3857')", 0, 0, 0);
    sqlite3_close(db);
    terrain_pack_close(&p);
    assert(sqlite3_open(argv[2], &db) == SQLITE_OK);
    assert(sqlite3_exec(db, "update tiles set crc=crc+1 where style=1 and lod=0 and col=0 and row=0", 0, 0,
                        0) == SQLITE_OK);
    sqlite3_close(db);
    assert(terrain_pack_open(&p, argv[2], err, sizeof(err)));
    assert(!terrain_pack_tile(&p, 1, 0, 0, 0, rgba));
    assert(terrain_pack_tile(&p, 0, 0, 0, 0, rgba));
    terrain_pack_close(&p);
    assert(sqlite3_open(argv[2], &db) == SQLITE_OK);
    sqlite3_exec(db, "pragma application_id=0", 0, 0, 0);
    sqlite3_close(db);
    assert(!terrain_pack_open(&p, argv[2], err, sizeof(err)));
    free(rgba);
    printf("terrain admission and all %d tile CRCs passed; corrupt replacement rejected\n", n);
}
