#include "app/app_internal.h"

bool app_polygon_fill_gate_allows(uint32_t expected_tiles, uint32_t ready_tiles) {
    if (expected_tiles == 0u || ready_tiles == 0u) {
        return false;
    }
    if (ready_tiles >= expected_tiles) {
        return true;
    }

    /*
     * Retained polygon fills are visually tile-sized. Require high coverage
     * before enabling them so a layer does not alternate between filled
     * rectangles and outline-only neighbors while assets are warming.
     */
    uint64_t lhs = (uint64_t)ready_tiles * 100u;
    uint64_t rhs = (uint64_t)expected_tiles * 90u;
    return lhs >= rhs;
}

bool app_polygon_fill_screen_gate_allows(uint32_t expected_tiles, uint32_t ready_tiles) {
    if (expected_tiles == 0u || ready_tiles == 0u) {
        return false;
    }
    return ready_tiles >= expected_tiles;
}
