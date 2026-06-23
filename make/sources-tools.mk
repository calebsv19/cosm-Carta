TOOL_TARGET := $(TOOL_BIN_DIR)/mapforge_region
TOOL_SRCS := tools/mapforge_region.c tools/mapforge_region_source.c tools/mapforge_region_tile_build.c tools/mapforge_region_tile_files.c tools/mapforge_region_archive_meta.c tools/mapforge_region_metrics_dataset.c tools/mapforge_region_publish.c tools/mapforge_region_layers.c tools/mapforge_publish_support.c src/map/mercator.c src/map/tile_math.c src/core/log.c
REGION_VALIDATE_TARGET := $(TOOL_BIN_DIR)/mapforge_region_validate
REGION_VALIDATE_SRCS := tools/mapforge_region_validate.c src/app/region.c src/app/region_loader.c src/map/tile_source.c src/core/log.c
GRAPH_TARGET := $(TOOL_BIN_DIR)/mapforge_graph
GRAPH_SRCS := tools/mapforge_graph.c tools/mapforge_graph_source.c tools/mapforge_graph_output.c tools/mapforge_publish_support.c src/map/mercator.c src/core/log.c
