#include "app/app_tile_render_internal.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(!app_polygon_fill_gate_allows(0u, 0u));
    assert(!app_polygon_fill_gate_allows(8u, 0u));
    assert(!app_polygon_fill_gate_allows(10u, 8u));
    assert(app_polygon_fill_gate_allows(10u, 9u));
    assert(app_polygon_fill_gate_allows(10u, 10u));
    assert(!app_polygon_fill_gate_allows(3u, 2u));
    assert(app_polygon_fill_gate_allows(3u, 3u));
    assert(app_polygon_fill_gate_allows(50u, 45u));
    assert(!app_polygon_fill_gate_allows(50u, 44u));

    assert(!app_polygon_fill_screen_gate_allows(0u, 0u));
    assert(!app_polygon_fill_screen_gate_allows(10u, 0u));
    assert(!app_polygon_fill_screen_gate_allows(10u, 9u));
    assert(app_polygon_fill_screen_gate_allows(10u, 10u));
    assert(!app_polygon_fill_screen_gate_allows(50u, 49u));
    assert(app_polygon_fill_screen_gate_allows(50u, 50u));

    printf("app_polygon_fill_gate_test: success\n");
    return 0;
}
