#ifndef MAPFORGE_APP_ROUTE_INTERNAL_H
#define MAPFORGE_APP_ROUTE_INTERNAL_H

#include "app/app_internal.h"

void app_route_result_clear(RouteComputeResult *result);
void app_route_snap_index_free(RouteSnapIndex *index);
bool app_route_snap_index_build(const RouteGraph *graph, RouteSnapIndex *index);
void *app_route_worker_thread_main(void *userdata);

#endif
