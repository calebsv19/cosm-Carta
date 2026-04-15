#ifndef MAPFORGE_APP_ROUTE_INTERNAL_H
#define MAPFORGE_APP_ROUTE_INTERNAL_H

#include "app/app_internal.h"

void app_route_result_clear(RouteComputeResult *result);
void app_route_snap_index_free(RouteSnapIndex *index);
bool app_route_snap_index_build(const RouteGraph *graph, RouteSnapIndex *index);
void app_route_apply_worker_result(AppState *app, RouteComputeResult *result);
void app_route_apply_graph_result(AppState *app,
                                  bool have_graph_result,
                                  bool graph_result_ok,
                                  uint32_t graph_result_request_id,
                                  RouteState *graph_result_state,
                                  RouteSnapIndex *graph_result_snap_index);
void *app_route_worker_thread_main(void *userdata);

#endif
