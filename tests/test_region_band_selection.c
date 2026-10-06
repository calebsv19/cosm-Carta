#include "app/region.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Models an archive whose coarse/mid/fine bands occupy distinct source zoom ranges.
static void seed_coverage(RegionInfo *region) {
    memset(region, 0, sizeof(*region));
    region->has_tile_coverage = true;
    for (unsigned layer = 0; layer < REGION_ROLLUP_LAYER_COUNT; ++layer) {
        for (unsigned z = 10; z <= 15; ++z) {
            unsigned band = z < 12 ? REGION_ROLLUP_BAND_COARSE
                                  : z < 14 ? REGION_ROLLUP_BAND_MID : REGION_ROLLUP_BAND_FINE;
            RegionTileCoverageZoom *c = &region->tile_coverage[band][layer][z];
            c->has_tiles = true;
            c->tile_count = 1;
            c->min_x = c->max_x = 1;
            c->min_y = c->max_y = 1;
        }
    }
}
// Verifies every policy preference resolves to real coverage, including the fit-view boundary failure.
int main(void) {
    RegionInfo region;
    seed_coverage(&region);
    for (int kind = 0; kind < TILE_LAYER_COUNT; ++kind) {
        for (uint16_t z = 10; z <= 15; ++z) {
            for (int band = 0; band < TILE_BAND_COUNT; ++band) {
                TileZoomBand resolved = region_tile_coverage_resolve_band(
                    &region, (TileLayerKind)kind, (TileZoomBand)band, z);
                assert(region_tile_coverage_contains(&region, (TileLayerKind)kind, resolved,
                                                     (TileCoord){z, 1, 1}));
            }
        }
    }
    assert(region_tile_coverage_resolve_band(&region, TILE_LAYER_ROAD_LOCAL, TILE_BAND_MID, 14) == TILE_BAND_FINE);
    assert(region_tile_coverage_resolve_band(&region, TILE_LAYER_POLY_WATER, TILE_BAND_COARSE, 12) == TILE_BAND_MID);
    assert(region_tile_coverage_resolve_band(&region, TILE_LAYER_POLY_BUILDING, TILE_BAND_FINE, 13) == TILE_BAND_MID);
    assert(region_tile_coverage_resolve_band(&region, TILE_LAYER_ROAD_LOCAL, TILE_BAND_MID, 9) == TILE_BAND_MID);
    assert(!region_tile_coverage_has_zoom(&region, TILE_LAYER_ROAD_LOCAL, TILE_BAND_MID, 9));
    region.tile_coverage[REGION_ROLLUP_BAND_MID][REGION_ROLLUP_LAYER_LOCAL][14] =
        region.tile_coverage[REGION_ROLLUP_BAND_FINE][REGION_ROLLUP_LAYER_LOCAL][14];
    assert(region_tile_coverage_resolve_band(&region, TILE_LAYER_ROAD_LOCAL, TILE_BAND_MID, 14) == TILE_BAND_MID);
    region.has_tile_coverage = false;
    assert(region_tile_coverage_resolve_band(&region, TILE_LAYER_ROAD_LOCAL, TILE_BAND_COARSE, 14) == TILE_BAND_COARSE);
    assert(region_tile_coverage_resolve_band(NULL, TILE_LAYER_ROAD_LOCAL, TILE_BAND_COARSE, 14) == TILE_BAND_COARSE);
    puts("region band selection: source coverage, preferred-band preservation and unknown coverage passed");
    return 0;
}
