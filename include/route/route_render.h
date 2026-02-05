#ifndef MAPFORGE_ROUTE_ROUTE_RENDER_H
#define MAPFORGE_ROUTE_ROUTE_RENDER_H

#include "camera/camera.h"
#include "render/renderer.h"
#include "route/astar.h"
#include "route/graph.h"

#include <stdbool.h>
#include <stdint.h>

// Renders a computed route path with optional start/end markers.
void route_render_draw(Renderer *renderer, const Camera *camera, const RouteGraph *graph, const RoutePath *path, bool has_start, uint32_t start_node, bool has_goal, uint32_t goal_node);

#endif
