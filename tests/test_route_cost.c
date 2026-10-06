#include "route/astar.h"
#include "route/route.h"
#include "route/cost.h"
#include "map/geodesic.h"
#include "map/mercator.h"
#include "map/mft_loader.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Provides an independent non-heuristic shortest-path oracle for every objective/mode.
static float oracle(const RouteGraph *g, uint32_t start, uint32_t goal, RouteObjective objective, RouteTravelMode mode) {
    float d[8]; bool seen[8] = {false};
    for (uint32_t n = 0; n < g->node_count; ++n) d[n] = FLT_MAX;
    d[start] = 0;
    for (uint32_t round = 0; round < g->node_count; ++round) {
        uint32_t at = UINT32_MAX;
        for (uint32_t n = 0; n < g->node_count; ++n) if (!seen[n] && (at == UINT32_MAX || d[n] < d[at])) at = n;
        if (at == UINT32_MAX || d[at] == FLT_MAX) break;
        seen[at] = true;
        for (uint32_t e = g->edge_start[at]; e < g->edge_start[at + 1]; ++e) {
            if (!route_class_allowed(mode, g->edge_class[e])) continue;
            float length = g->edge_length[e];
            float time = length / (mode == ROUTE_MODE_WALK ? 1.4f : fmaxf(0.1f, g->edge_speed[e]));
            float weight = length;
            if (objective == ROUTE_OBJECTIVE_LOWEST_TIME) weight = time;
            if (objective == ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN) weight = length + fmaxf(0, g->edge_grade[e]) * length * 8 + g->edge_penalty[e] * length * 0.5f;
            if (objective == ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD) weight = time * (mode == ROUTE_MODE_WALK ? 1 : g->edge_speed_limit[e] >= 13.4112f ? 0.6f : 1.8f);
            uint32_t to = g->edge_to[e];
            if (d[at] + weight < d[to]) d[to] = d[at] + weight;
        }
    }
    return d[goal];
}

// Creates deterministic geographically valid graphs with parallel roads and mixed access classes.
static RouteGraph fixture(unsigned seed) {
    RouteGraph g = {0}; g.node_count = 6; g.edge_count = 36;
    g.node_x = calloc(6, sizeof(double)); g.node_y = calloc(6, sizeof(double));
    g.edge_start = calloc(7, sizeof(uint32_t)); g.edge_to = calloc(36, sizeof(uint32_t));
    g.edge_length = calloc(36, sizeof(float)); g.edge_speed = calloc(36, sizeof(float));
    g.edge_speed_limit = calloc(36, sizeof(float)); g.edge_grade = calloc(36, sizeof(float));
    g.edge_penalty = calloc(36, sizeof(float)); g.edge_class = calloc(36, sizeof(uint8_t));
    for (uint32_t n = 0; n < 6; ++n) {
        MercatorMeters p = mercator_from_latlon((LatLon){47.66 + n * 0.002, -122.35 + (n % 3) * 0.003});
        g.node_x[n] = p.x; g.node_y[n] = p.y;
        g.edge_start[n] = n * 6;
    }
    g.edge_start[6] = 36;
    for (uint32_t n = 0; n < 6; ++n) for (uint32_t k = 0; k < 6; ++k) {
        uint32_t e = n * 6 + k, to = (n + 1 + (k % 5)) % 6;
        g.edge_to[e] = to;
        g.edge_length[e] = (float)mercator_ground_distance_meters((MercatorMeters){g.node_x[n], g.node_y[n]}, (MercatorMeters){g.node_x[to], g.node_y[to]});
        g.edge_speed[e] = 2 + (seed * 13 + e * 7) % 27;
        g.edge_speed_limit[e] = g.edge_speed[e];
        g.edge_grade[e] = ((int)((seed + e) % 7) - 3) * 0.04f;
        g.edge_penalty[e] = (e % 3) * 0.15f;
        g.edge_class[e] = k == 0 ? ROAD_CLASS_RESIDENTIAL : (uint8_t)((seed + e) % 9);
    }
    return g;
}

// Writes graph versions directly to verify compatibility and strict metric validation.
static void write_fixture(const char *path, const RouteGraph *g, uint32_t version, bool bad) {
    FILE *f = fopen(path, "wb"); assert(f);
    fwrite("MFG1", 1, 4, f); fwrite(&version, 4, 1, f);
    fwrite(&g->node_count, 4, 1, f); fwrite(&g->edge_count, 4, 1, f);
    fwrite(g->node_x, 8, g->node_count, f); fwrite(g->node_y, 8, g->node_count, f);
    fwrite(g->edge_start, 4, g->node_count + 1, f); fwrite(g->edge_to, 4, g->edge_count, f);
    for (uint32_t e = 0; e < g->edge_count; ++e) {
        float value = g->edge_length[e] * (version < 3 || bad ? 1.485f : 1);
        fwrite(&value, 4, 1, f);
    }
    fwrite(g->edge_speed, 4, g->edge_count, f); fwrite(g->edge_class, 1, g->edge_count, f);
    if (version >= 2) {
        fwrite(g->edge_speed_limit, 4, g->edge_count, f);
        fwrite(g->edge_grade, 4, g->edge_count, f); fwrite(g->edge_penalty, 4, g->edge_count, f);
    }
    if (version >= 4) for (uint32_t e = 0; e < g->edge_count; ++e) {
        uint8_t access = (route_class_allowed(ROUTE_MODE_CAR, g->edge_class[e]) ? 1u : 0u) | (route_class_allowed(ROUTE_MODE_WALK, g->edge_class[e]) ? 2u : 0u);
        fwrite(&access, 1, 1, f);
    }
    assert(fclose(f) == 0);
}

// Exercises a directed walk/car/walk chain and verifies legal orientation and mode-aware totals.
static void mixed_legs(void) {
    RouteState state = {0}; route_state_init(&state); state.graph = fixture(4); state.loaded = true;
    RouteGraph *g = &state.graph;
    /* Only edges 0->1 foot, 1->2 car, 2->3 car, 3->4 foot remain. */
    uint32_t to[] = {1, 2, 3, 4}; uint32_t starts[] = {0, 1, 2, 3, 4, 4, 4};
    memcpy(g->edge_start, starts, sizeof(starts)); memcpy(g->edge_to, to, sizeof(to)); g->edge_count = 4;
    for (uint32_t e = 0; e < 4; ++e) {
        g->edge_class[e] = e == 0 || e == 3 ? ROAD_CLASS_FOOTWAY : ROAD_CLASS_RESIDENTIAL;
        g->edge_length[e] = 100; g->edge_speed[e] = 10;
    }
    /* Goal transfer at 3 must be detected even though its drive edge is incoming only. */
    assert(route_state_route(&state, 0, 4));
    assert(state.path.nodes[0] == 0 && state.path.nodes[state.path.count - 1] == 4);
    assert(fabsf(state.path.total_length_m - 400) < 0.01f);
    assert(fabsf(state.path.total_time_s - (200 / 1.4f + 20)) < 0.01f);
    assert(state.path.edge_modes[0] == ROUTE_MODE_WALK && state.path.edge_modes[1] == ROUTE_MODE_CAR && state.path.edge_modes[3] == ROUTE_MODE_WALK);
    route_state_shutdown(&state);
}

// Prevents forbidden/slow parallel edges from winning endpoint segment selection by file order.
static void parallel_endpoints(void) {
    RouteGraph g = fixture(0);
    g.edge_access = calloc(g.edge_count, sizeof(uint8_t));
    for (uint32_t edge = 0; edge < g.edge_count; ++edge) g.edge_access[edge] = 3u;
    g.edge_access[0] = 2u;
    g.edge_speed[0] = 50.0f;
    g.edge_speed[5] = 10.0f;
    uint32_t edge = UINT32_MAX;
    assert(route_find_best_edge(&g, 0, 1, ROUTE_OBJECTIVE_LOWEST_TIME, ROUTE_MODE_CAR, &edge));
    assert(edge == 5u);
    g.edge_access[0] = 3u;
    assert(route_find_best_edge(&g, 0, 1, ROUTE_OBJECTIVE_LOWEST_TIME, ROUTE_MODE_CAR, &edge));
    assert(edge == 0u);
    g.edge_grade[0] = 0.2f; g.edge_grade[5] = 0.0f; g.edge_penalty[5] = 0.0f;
    assert(route_find_best_edge(&g, 0, 1, ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN, ROUTE_MODE_WALK, &edge));
    assert(edge == 5u);
    g.edge_access[0] = 1u; g.edge_access[5] = 1u;
    assert(!route_find_best_edge(&g, 0, 1, ROUTE_OBJECTIVE_SHORTEST_DISTANCE, ROUTE_MODE_WALK, &edge));
    route_graph_free(&g);
}

int main(void) {
    double d = ground_distance_meters((LatLon){47.66, -122.35}, (LatLon){47.669, -122.35});
    assert(fabs(d - 1001.8754) < 0.01);
    for (unsigned seed = 0; seed < 80; ++seed) {
        RouteGraph g = fixture(seed);
        for (unsigned mode = 0; mode < 2; ++mode) for (unsigned objective = 0; objective < 4; ++objective) {
            RoutePath p = {0}; float wanted = oracle(&g, 0, 5, (RouteObjective)objective, (RouteTravelMode)mode);
            assert(route_astar(&g, 0, 5, (RouteObjective)objective, (RouteTravelMode)mode, &p));
            float got = route_path_objective_cost(&g, &p, (RouteObjective)objective, (RouteTravelMode)mode);
            assert(fabsf(got - wanted) < fmaxf(0.02f, wanted * 0.00001f));
            assert(p.cumulative_time_s[p.count - 1] == p.total_time_s);
            if (mode == ROUTE_MODE_WALK) assert(fabsf(p.total_time_s - p.total_length_m / 1.4f) < 0.01f);
            route_path_free(&p);
        }
        route_graph_free(&g);
    }
    RouteGraph g = fixture(3), loaded = {0}; char path[] = "/tmp/carta-cost-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0); close(fd);
    for (uint32_t version = 1; version <= 4; ++version) {
        write_fixture(path, &g, version, false); assert(route_graph_load(path, &loaded));
        assert(loaded.legacy_lengths_normalized == (version < 3));
        for (uint32_t e = 0; e < g.edge_count; ++e) assert(fabsf(loaded.edge_length[e] - g.edge_length[e]) < 0.01f);
        route_graph_free(&loaded);
    }
    write_fixture(path, &g, 3, true); assert(!route_graph_load(path, &loaded));
    unlink(path); route_graph_free(&g);
    mixed_legs();
    parallel_endpoints();
    puts("route cost: ground metric, legacy/v3/v4 validation, 640 oracle routes, directed mixed legs and parallel endpoint permissions/objectives passed");
    return 0;
}
