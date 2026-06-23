#include "route_preview_test_fixture.h"

#include <string.h>

void mapforge_test_route_fixture_seed_corner(RouteGraph *graph, RoutePath *path) {
    static double node_x[] = {0.0, 0.0, 10.0};
    static double node_y[] = {0.0, 10.0, 10.0};
    static uint32_t nodes[] = {0u, 1u, 2u};
    static float cumulative_time_s[] = {0.0f, 10.0f, 20.0f};

    if (!graph || !path) {
        return;
    }
    memset(graph, 0, sizeof(*graph));
    memset(path, 0, sizeof(*path));
    graph->node_count = 3u;
    graph->node_x = node_x;
    graph->node_y = node_y;
    path->nodes = nodes;
    path->count = 3u;
    path->cumulative_time_s = cumulative_time_s;
    path->total_time_s = 20.0f;
}

void mapforge_test_route_fixture_seed_sway(RouteGraph *graph, RoutePath *path) {
    static double node_x[] = {0.0, 0.0, 1.0, 0.0};
    static double node_y[] = {0.0, 10.0, 20.0, 30.0};
    static uint32_t nodes[] = {0u, 1u, 2u, 3u};
    static float cumulative_time_s[] = {0.0f, 10.0f, 20.0f, 30.0f};

    if (!graph || !path) {
        return;
    }
    memset(graph, 0, sizeof(*graph));
    memset(path, 0, sizeof(*path));
    graph->node_count = 4u;
    graph->node_x = node_x;
    graph->node_y = node_y;
    path->nodes = nodes;
    path->count = 4u;
    path->cumulative_time_s = cumulative_time_s;
    path->total_time_s = 30.0f;
}
