TOOL_TARGET := $(TOOL_BIN_DIR)/mapforge_region
TOOL_SRCS := tools/mapforge_region.c tools/mapforge_region_source.c tools/mapforge_region_tile_build.c tools/mapforge_region_tile_files.c tools/mapforge_region_archive_meta.c tools/mapforge_region_metrics_dataset.c tools/mapforge_region_publish.c tools/mapforge_region_layers.c tools/mapforge_publish_support.c src/map/mercator.c src/map/tile_math.c src/core/log.c
REGION_VALIDATE_TARGET := $(TOOL_BIN_DIR)/mapforge_region_validate
REGION_VALIDATE_SRCS := tools/mapforge_region_validate.c src/app/region.c src/app/region_loader.c src/map/tile_source.c src/core/log.c
GRAPH_TARGET := $(TOOL_BIN_DIR)/mapforge_graph
GRAPH_SRCS := tools/mapforge_graph.c tools/mapforge_graph_source.c tools/mapforge_graph_output.c tools/mapforge_publish_support.c src/map/mercator.c src/map/geodesic.c src/route/graph.c src/core/log.c

PLACES_TARGET := $(TOOL_BIN_DIR)/mapforge_places
PLACES_SRCS := src/app/history/history_gpx.c src/app/history/history_store.c src/app/history/history_commands.c src/app/history/history_status.c tools/mapforge_places.c src/app/pins/app_pins.c src/app/pins/app_pins_workspace.c src/app/pins/place_repository.c src/app/pins/place_commands.c src/app/pins/place_routes.c src/app/pins/place_maintenance.c src/app/pins/place_profile.c src/route/query.c src/route/route.c src/route/astar.c src/route/cost.c src/route/graph.c src/map/mercator.c src/map/geodesic.c src/core/log.c
