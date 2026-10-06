#ifndef MAPFORGE_ROUTE_QUERY_H
#define MAPFORGE_ROUTE_QUERY_H
#include "route/route.h"
#include "map/mercator.h"
// Stores a road projection independently of the requested home marker.
typedef struct RouteEndpointAnchor {
    double requested_x;
    double requested_y;
    double connector_m;
    bool valid;
    bool on_edge;
    uint32_t node;
    uint32_t edge_from;
    uint32_t edge_to;
    float world_x;
    float world_y;
    float dist_to_from_m;
    float dist_to_to_m;
} RouteEndpointAnchor;
// Resolves the nearest permitted road segment within a physical distance limit.
bool route_query_snap(const RouteGraph *graph, MercatorMeters requested, RouteTravelMode mode, double limit_m,
                      RouteEndpointAnchor *out);
// Solves partial endpoints and retains actual connector/road geometry for every consumer.
bool route_query_solve(RouteState *state, const RouteEndpointAnchor *start, const RouteEndpointAnchor *goal);
#endif
