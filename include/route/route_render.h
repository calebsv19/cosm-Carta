#ifndef MAPFORGE_ROUTE_ROUTE_RENDER_H
#define MAPFORGE_ROUTE_ROUTE_RENDER_H

#include "camera/camera.h"
#include "render/renderer.h"
#include "route/astar.h"
#include "route/graph.h"

#include <stdbool.h>
#include <stdint.h>

// Renders computed route paths with optional start/end markers.
void route_render_draw(Renderer *renderer, const Camera *camera, const RouteGraph *graph, const RoutePath *path, const RoutePath *drive_path, const RoutePath *walk_path, bool has_start, uint32_t start_node, bool has_goal, uint32_t goal_node, bool has_transfer, uint32_t transfer_node);

#endif
