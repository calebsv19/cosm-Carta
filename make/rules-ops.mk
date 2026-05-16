MIN_Z ?= 10
MAX_Z ?= 18
RENDER_BACKEND ?= vulkan
VK_DEBUG ?= 0
OSM_DIR ?= $(HOME)/Desktop/osm_maps
REGIONS_DIR ?= data/regions
MAPFORGE_REGIONS_DIR ?= $(REGIONS_DIR)
PACKAGE_REGIONS_SRC ?= $(MAPFORGE_REGIONS_DIR)
BATCH_MODE ?= missing
BATCH_REGION ?=
BATCH_OSM ?=
BATCH_EXTRA_FLAGS ?=
PRUNE_DAYS ?= 30
KEEP_OLD ?= 1
REPLACE ?= 0
PRUNE_DRY_RUN ?= 0
PAD_BOUNDS ?= 0
EMIT_CONTOUR_EMPTY ?= 0
EMIT_LEGACY_TILES ?= 1
EMIT_ARCHIVE ?= 0
ARCHIVE_PATH ?= tiles.mbtiles

REGION_TOOL_FLAGS := $(if $(filter 1,$(REPLACE)),--replace,) \
	--keep-old $(KEEP_OLD) \
	--prune-days $(PRUNE_DAYS) \
	$(if $(filter 1,$(PRUNE_DRY_RUN)),--prune-dry-run,) \
	$(if $(filter 1,$(PAD_BOUNDS)),--pad-bounds,) \
	$(if $(filter 1,$(EMIT_CONTOUR_EMPTY)),--emit-contour-empty,) \
	$(if $(filter 1,$(EMIT_LEGACY_TILES)),--emit-legacy-tiles,--no-legacy-tiles) \
	$(if $(filter 1,$(EMIT_ARCHIVE)),--emit-archive --archive-path $(ARCHIVE_PATH),)

GRAPH_TOOL_FLAGS := $(if $(filter 1,$(REPLACE)),--replace,) \
	--keep-old $(KEEP_OLD) \
	--prune-days $(PRUNE_DAYS) \
	$(if $(filter 1,$(PRUNE_DRY_RUN)),--prune-dry-run,)


route: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" $(GRAPH_TOOL_FLAGS)

region: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

region-archive:
	$(MAKE) --no-print-directory region EMIT_ARCHIVE=1 EMIT_LEGACY_TILES=0

region-validate: tools-build
	./$(REGION_VALIDATE_TARGET) $(if $(REGION),--region $(REGION),) $(if $(filter 1,$(STRICT)),--strict,)

region-rebuild: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) --replace $(REGION_TOOL_FLAGS)

region-rebuild-archive:
	$(MAKE) --no-print-directory region-rebuild EMIT_ARCHIVE=1 EMIT_LEGACY_TILES=0

route-rebuild: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" --replace $(GRAPH_TOOL_FLAGS)

tools-progress: tools

graph-progress: graph

region-progress:
	tools/run_with_progress.sh --label "region $(REGION)" ./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) $(if $(DEM),--dem $(DEM),) --out "$(REGIONS_DIR)/$(REGION)" --min-z $(MIN_Z) --max-z $(MAX_Z) $(REGION_TOOL_FLAGS)

region-progress-archive:
	$(MAKE) --no-print-directory region-progress EMIT_ARCHIVE=1 EMIT_LEGACY_TILES=0

route-progress:
	tools/run_with_progress.sh --label "route $(REGION)" ./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out "$(REGIONS_DIR)/$(REGION)" $(GRAPH_TOOL_FLAGS)

batch-regions:
	tools/build_regions.sh --osm-dir "$(OSM_DIR)" --regions-dir "$(REGIONS_DIR)" --min-z $(MIN_Z) --max-z $(MAX_Z) --keep-old $(KEEP_OLD) --prune-days $(PRUNE_DAYS) \
	$(if $(filter all,$(BATCH_MODE)),--all,--missing) \
	$(if $(BATCH_REGION),--region "$(BATCH_REGION)",) \
	$(if $(BATCH_OSM),--osm "$(BATCH_OSM)",) \
	$(BATCH_EXTRA_FLAGS)

disk-usage:
	@echo "=== MapForge Disk Usage ==="
	@du -sh build "$(REGIONS_DIR)" ide_files 2>/dev/null || true
	@echo ""
	@echo "=== Top Region Dirs ==="
	@if [ -d "$(REGIONS_DIR)" ] && [ -n "$$(ls -A "$(REGIONS_DIR)" 2>/dev/null)" ]; then \
		du -sh "$(REGIONS_DIR)"/* 2>/dev/null | sort -h; \
	else \
		echo "no regions present under $(REGIONS_DIR)"; \
	fi

region-clean:
	@if [ -z "$(REGION)" ]; then echo "REGION is required (example: make region-clean REGION=seattle)"; exit 1; fi
	@if ! printf "%s\n" "$(REGION)" | rg -q '^[A-Za-z0-9_.-]+$$'; then echo "invalid REGION name: $(REGION)"; exit 1; fi
	rm -rf "$(REGIONS_DIR)/$(REGION)"

graph-clean:
	@if [ -z "$(REGION)" ]; then echo "REGION is required (example: make graph-clean REGION=seattle)"; exit 1; fi
	@if ! printf "%s\n" "$(REGION)" | rg -q '^[A-Za-z0-9_.-]+$$'; then echo "invalid REGION name: $(REGION)"; exit 1; fi
	rm -rf "$(REGIONS_DIR)/$(REGION)/graph"

prune-regions:
	tools/prune_regions.sh --regions-dir "$(REGIONS_DIR)" --prune-days "$(PRUNE_DAYS)" --keep-old "$(KEEP_OLD)" $(if $(filter 1,$(PRUNE_DRY_RUN)),--dry-run,)

shared-check:
	@echo "=== Shared Library Check ==="
	@for path in "$(CORE_BASE_LIB)" "$(CORE_IO_LIB)" "$(CORE_DATA_LIB)" "$(CORE_SPACE_LIB)" "$(CORE_PACK_LIB)" "$(CORE_TIME_LIB)" "$(CORE_QUEUE_LIB)" "$(CORE_SCHED_LIB)" "$(CORE_JOBS_LIB)" "$(CORE_WORKERS_LIB)" "$(CORE_WAKE_LIB)" "$(CORE_KERNEL_LIB)" "$(CORE_TRACE_LIB)" "$(CORE_VIEWPORT2D_LIB)"; do \
		if [ ! -f "$$path" ]; then \
			echo "missing: $$path"; \
			exit 1; \
		fi; \
		echo "ok: $$path"; \
	done
	@echo ""
	@echo "=== Shared Versions ==="
	@for path in "$(CORE_BASE_DIR)/VERSION" "$(CORE_IO_DIR)/VERSION" "$(CORE_DATA_DIR)/VERSION" "$(CORE_SPACE_DIR)/VERSION" "$(CORE_PACK_DIR)/VERSION" "$(CORE_TIME_DIR)/VERSION" "$(CORE_QUEUE_DIR)/VERSION" "$(CORE_SCHED_DIR)/VERSION" "$(CORE_JOBS_DIR)/VERSION" "$(CORE_WORKERS_DIR)/VERSION" "$(CORE_WAKE_DIR)/VERSION" "$(CORE_KERNEL_DIR)/VERSION" "$(CORE_TRACE_DIR)/VERSION" "$(CORE_VIEWPORT2D_DIR)/VERSION"; do \
		if [ -f "$$path" ]; then \
			printf "%s: " "$$path"; cat "$$path"; \
		else \
			echo "$$path: missing"; \
			exit 1; \
		fi; \
	done

trace-latest:
	@latest=$$(ls -1t build/traces/*.pack 2>/dev/null | head -n 1); \
	if [ -z "$$latest" ]; then \
		echo "no trace packs found under build/traces"; \
		exit 1; \
	fi; \
	echo "inspecting $$latest"; \
	$(MAKE) -C $(CORE_PACK_DIR) tools >/dev/null; \
	$(CORE_PACK_DIR)/build/pack_cli inspect "$$latest"

vk-lib:
	@if [ ! -f "$(VK_RENDERER_RESOLVED_DIR)/include/vk_renderer.h" ]; then \
		echo "vk renderer not found at $(VK_RENDERER_DIR)"; \
		exit 1; \
	fi
	$(MAKE) -C "$(VK_RENDERER_RESOLVED_DIR)" all
	@mkdir -p $(dir $(VK_BUILD_LIB)) $(VK_BUILD_SHADER_DIR)
	@cp "$(VK_RENDERER_RESOLVED_DIR)/build/lib/libvkrenderer.a" "$(VK_BUILD_LIB)"
	@for shader in $(VK_REQUIRED_SHADERS); do \
		cp "$(VK_RENDERER_RESOLVED_DIR)/shaders/$$shader" "$(VK_BUILD_SHADER_DIR)/$$shader"; \
	done

vk-check: vk-lib
	@echo "vk renderer dir: $(VK_RENDERER_RESOLVED_DIR)"
	@echo "checking required vk symbols..."
	@nm -g "$(VK_BUILD_LIB)" | rg -q "vk_renderer_init" || (echo "missing symbol: vk_renderer_init" && exit 1)
	@nm -g "$(VK_BUILD_LIB)" | rg -q "vk_renderer_begin_frame" || (echo "missing symbol: vk_renderer_begin_frame" && exit 1)
	@nm -g "$(VK_BUILD_LIB)" | rg -q "vk_renderer_end_frame" || (echo "missing symbol: vk_renderer_end_frame" && exit 1)
	@echo "checking required shaders..."
	@for shader in $(VK_REQUIRED_SHADERS); do \
		test -f "$(VK_BUILD_SHADER_DIR)/$$shader" || (echo "missing shader: $$shader" && exit 1); \
	done
	@echo "vulkan checks passed."

