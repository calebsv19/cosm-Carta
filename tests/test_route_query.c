#include "map/geodesic.h"
#include "route/query.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

// Verifies partial segments, same-road direction, connectors and physical snap limits.
int main(void) {
    MercatorMeters left = mercator_from_latlon((LatLon){47.66, -122.31}),
                   right = mercator_from_latlon((LatLon){47.66, -122.306});
    double x[2] = {left.x, right.x}, y[2] = {left.y, right.y};
    uint32_t starts[3] = {0, 1, 2}, to[2] = {1, 0};
    float length = (float)mercator_ground_distance_meters(left, right), lengths[2] = {length, length},
          speed[2] = {10, 10};
    uint8_t classes[2] = {5, 5}, access[2] = {3, 3};
    RouteState state;
    route_state_init(&state);
    state.loaded = true;
    state.mode = ROUTE_MODE_WALK;
    state.objective = ROUTE_OBJECTIVE_SHORTEST_DISTANCE;
    state.graph = (RouteGraph){.node_count = 2,
                               .edge_count = 2,
                               .node_x = x,
                               .node_y = y,
                               .edge_start = starts,
                               .edge_to = to,
                               .edge_length = lengths,
                               .edge_speed = speed,
                               .edge_class = classes,
                               .edge_access = access};
    MercatorMeters p = {left.x + (right.x - left.x) * 0.25, left.y + 10},
                   q = {left.x + (right.x - left.x) * 0.75, left.y + 10};
    RouteEndpointAnchor a, b;
    assert(route_query_snap(&state.graph, p, state.mode, 200, &a));
    assert(route_query_snap(&state.graph, q, state.mode, 200, &b));
    assert(route_query_solve(&state, &a, &b));
    double expected = length * 0.5 + a.connector_m + b.connector_m;
    assert(fabs(state.path.total_length_m - expected) < 1.0);
    assert(fabs(state.path.total_time_s - expected / 1.4) < 1.0);
    assert(state.path.count == 4 && fabs(route_path_x(&state.graph, &state.path, 0) - p.x) < 0.001);
    assert(fabs(route_path_x(&state.graph, &state.path, 3) - q.x) < 0.001);
    assert(route_query_solve(&state, &b, &a));
    assert(fabs(state.path.total_length_m - expected) < 1.0);
    assert(route_query_solve(&state, &a, &a));
    assert(state.path.total_length_m == 0 && state.path.total_time_s == 0);
    state.mode = ROUTE_MODE_CAR;
    access[0] = 1;
    access[1] = 0;
    assert(route_query_snap(&state.graph, p, state.mode, 200, &a));
    assert(route_query_snap(&state.graph, q, state.mode, 200, &b));
    assert(route_query_solve(&state, &a, &b));
    assert(!route_query_solve(&state, &b, &a));
    assert(!route_query_snap(&state.graph, (MercatorMeters){p.x, p.y + 1000}, state.mode, 200, &a));
    assert(!route_query_snap(&state.graph, p, ROUTE_MODE_WALK, 200, &a));
    route_state_clear(&state);
    puts("route query: same-segment distance/time, connectors, one-way access and bounded snapping passed");
    return 0;
}
