#ifndef MAPFORGE_ROUTE_PREVIEW_TEST_FIXTURE_H
#define MAPFORGE_ROUTE_PREVIEW_TEST_FIXTURE_H

#include "route/route.h"

void mapforge_test_route_fixture_seed_corner(RouteGraph *graph, RoutePath *path);
void mapforge_test_route_fixture_seed_sway(RouteGraph *graph, RoutePath *path);

#endif
