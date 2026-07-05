#include "render/vk_polygon_fill_geometry_guard.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void test_landuse_tile_boundary_slab_is_rejected(void) {
    const uint16_t ring[] = {
        0u, 0u,
        4096u, 0u,
        4096u, 2048u,
        0u, 2048u
    };
    assert(!vk_polygon_fill_ring_allowed_for_retained_mesh(TILE_LAYER_POLY_LANDUSE, ring, 4u));
}

static void test_building_tile_boundary_slab_is_rejected(void) {
    const uint16_t ring[] = {
        0u, 256u,
        4096u, 256u,
        4096u, 1792u,
        0u, 1792u
    };
    assert(!vk_polygon_fill_ring_allowed_for_retained_mesh(TILE_LAYER_POLY_BUILDING, ring, 4u));
}

static void test_urban_fills_are_disabled_by_default(void) {
    const uint16_t ring[] = {
        100u, 100u,
        180u, 100u,
        180u, 180u,
        100u, 180u
    };
    unsetenv("MAPFORGE_RETAINED_URBAN_FILLS");
    assert(!vk_polygon_fill_kind_allowed_for_retained_mesh(TILE_LAYER_POLY_LANDUSE));
    assert(!vk_polygon_fill_kind_allowed_for_retained_mesh(TILE_LAYER_POLY_BUILDING));
    assert(!vk_polygon_fill_ring_allowed_for_retained_mesh(TILE_LAYER_POLY_BUILDING, ring, 4u));
}

static void test_opt_in_preserves_interior_urban_shapes(void) {
    const uint16_t building[] = {
        100u, 100u,
        180u, 100u,
        180u, 180u,
        100u, 180u
    };
    const uint16_t ring[] = {
        640u, 640u,
        3150u, 780u,
        3450u, 2200u,
        2480u, 3100u,
        860u, 2740u
    };
    setenv("MAPFORGE_RETAINED_URBAN_FILLS", "1", 1);
    assert(vk_polygon_fill_kind_allowed_for_retained_mesh(TILE_LAYER_POLY_LANDUSE));
    assert(vk_polygon_fill_kind_allowed_for_retained_mesh(TILE_LAYER_POLY_BUILDING));
    assert(vk_polygon_fill_ring_allowed_for_retained_mesh(TILE_LAYER_POLY_BUILDING, building, 4u));
    assert(vk_polygon_fill_ring_allowed_for_retained_mesh(TILE_LAYER_POLY_LANDUSE, ring, 5u));
    unsetenv("MAPFORGE_RETAINED_URBAN_FILLS");
}

static void test_park_fill_is_not_quarantined_by_slab_guard(void) {
    const uint16_t ring[] = {
        0u, 0u,
        4096u, 0u,
        4096u, 2048u,
        0u, 2048u
    };
    assert(vk_polygon_fill_ring_allowed_for_retained_mesh(TILE_LAYER_POLY_PARK, ring, 4u));
}

int main(void) {
    test_landuse_tile_boundary_slab_is_rejected();
    test_building_tile_boundary_slab_is_rejected();
    test_urban_fills_are_disabled_by_default();
    test_opt_in_preserves_interior_urban_shapes();
    test_park_fill_is_not_quarantined_by_slab_guard();

    printf("vk_polygon_fill_geometry_guard_test: success\n");
    return 0;
}
