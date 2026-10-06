MAP_SPACE_TEST_TARGET := $(TEST_BIN_DIR)/map_space_test
MAP_SPACE_TEST_SRCS := tests/map_space_test.c src/map/map_space.c src/map/tile_math.c src/map/mercator.c src/camera/camera.c src/camera/camera_viewport_bridge.c
SHARED_THEME_FONT_ADAPTER_TEST_TARGET := $(TEST_BIN_DIR)/shared_theme_font_adapter_test
SHARED_THEME_FONT_ADAPTER_TEST_SRCS := tests/shared_theme_font_adapter_test.c src/ui/shared_theme_font_adapter.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c $(CORE_BASE_DIR)/src/core_base.c
MAP_TRACE_CONTRACT_TEST_TARGET := $(TEST_BIN_DIR)/map_trace_contract_test
MAP_TRACE_CONTRACT_TEST_SRCS := tests/map_trace_contract_test.c
APP_WORKER_CONTRACT_TEST_TARGET := $(TEST_BIN_DIR)/app_worker_contract_test
APP_WORKER_CONTRACT_TEST_SRCS := tests/app_worker_contract_test.c src/app/app_worker_contract.c
TILE_LOADER_SHUTDOWN_TEST_TARGET := $(TEST_BIN_DIR)/tile_loader_shutdown_test
TILE_LOADER_SHUTDOWN_TEST_SRCS := tests/tile_loader_shutdown_test.c src/map/tile_loader.c src/map/tile_source.c src/map/mft_loader.c src/map/polygon_cache.c src/map/polygon_triangulator.c src/core/log.c
TILE_SOURCE_ARCHIVE_TEST_TARGET := $(TEST_BIN_DIR)/tile_source_archive_test
TILE_SOURCE_ARCHIVE_TEST_SRCS := tests/tile_source_archive_test.c tests/tile_source_test_fixture.c src/map/tile_source.c
APP_ROUTE_SERVICE_TEST_TARGET := $(TEST_BIN_DIR)/app_route_service_test
APP_ROUTE_SERVICE_TEST_SRCS := tests/app_route_service_test.c src/app/route/app_route_service.c
APP_HEADLESS_PLAYBACK_TEST_TARGET := $(TEST_BIN_DIR)/app_headless_playback_test
APP_HEADLESS_PLAYBACK_TEST_SRCS := tests/app_headless_playback_test.c tests/route_preview_test_fixture.c src/app/headless/app_headless_playback.c
APP_ROUTE_PREVIEW_TEST_TARGET := $(TEST_BIN_DIR)/app_route_preview_test
APP_ROUTE_PREVIEW_TEST_SRCS := tests/app_route_preview_test.c tests/route_preview_test_fixture.c src/app/route/app_route_preview.c src/app/route/app_route_selection.c src/app/headless/app_headless_playback.c
APP_TILE_PRESENTER_POLICY_TEST_TARGET := $(TEST_BIN_DIR)/app_tile_presenter_policy_test
APP_TILE_PRESENTER_POLICY_TEST_SRCS := tests/app_tile_presenter_policy_test.c src/app/app_tile_presenter.c src/app/app_tile_lifecycle.c src/app/app_polygon_present_trace.c src/core/time.c
APP_POLYGON_FILL_GATE_TEST_TARGET := $(TEST_BIN_DIR)/app_polygon_fill_gate_test
APP_POLYGON_FILL_GATE_TEST_SRCS := tests/app_polygon_fill_gate_test.c src/app/app_polygon_fill_gate.c
VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_TARGET := $(TEST_BIN_DIR)/vk_polygon_fill_geometry_guard_test
VK_POLYGON_FILL_GEOMETRY_GUARD_TEST_SRCS := tests/vk_polygon_fill_geometry_guard_test.c src/render/vk_polygon_fill_geometry_guard.c
POLYGON_CACHE_GUARDRAILS_TEST_TARGET := $(TEST_BIN_DIR)/polygon_cache_guardrails_test
POLYGON_CACHE_GUARDRAILS_TEST_SRCS := tests/polygon_cache_guardrails_test.c src/map/polygon_cache.c src/map/polygon_triangulator.c
MFT_LOADER_SECURITY_TEST_TARGET := $(TEST_BIN_DIR)/mft_loader_security_test
MFT_LOADER_SECURITY_TEST_SRCS := tests/mft_loader_security_test.c src/map/mft_loader.c src/core/log.c
APP_RUNTIME_INPUT_POLICY_TEST_TARGET := $(TEST_BIN_DIR)/app_runtime_input_policy_test
APP_RUNTIME_INPUT_POLICY_TEST_SRCS := tests/app_runtime_input_policy_test.c src/app/app_runtime_input_policy.c
MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_TARGET := $(TEST_BIN_DIR)/map_forge_workspace_authoring_host_test
MAP_FORGE_WORKSPACE_AUTHORING_HOST_TEST_SRCS := tests/map_forge_workspace_authoring_host_test.c src/app/workspace_authoring/map_forge_workspace_authoring_host.c src/ui/shared_theme_font_adapter.c $(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c $(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c $(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c $(KIT_RENDER_DIR)/src/kit_render.c $(KIT_RENDER_DIR)/src/kit_render_backend_null.c $(KIT_RENDER_DIR)/src/kit_render_backend_vk.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c $(CORE_BASE_DIR)/src/core_base.c
APP_HEADER_LAYER_LAYOUT_TEST_TARGET := $(TEST_BIN_DIR)/app_header_layer_layout_test
APP_HEADER_LAYER_LAYOUT_TEST_SRCS := tests/app_header_layer_layout_test.c src/app/app_header_layer_layout.c
APP_RUNTIME_WINDOW_RESIZE_TEST_TARGET := $(TEST_BIN_DIR)/app_runtime_window_resize_test
APP_RUNTIME_WINDOW_RESIZE_TEST_SRCS := tests/app_runtime_window_resize_test.c src/app/app_runtime_window.c src/app/app_tile_state.c
APP_MAP_VIEWPORT_TEST_TARGET := $(TEST_BIN_DIR)/app_map_viewport_test
APP_MAP_VIEWPORT_TEST_SRCS := tests/app_map_viewport_test.c src/app/app_map_viewport.c src/camera/camera.c src/camera/camera_viewport_bridge.c src/map/mercator.c
APP_PIN_PANEL_LAYOUT_TEST_TARGET := $(TEST_BIN_DIR)/app_pin_panel_layout_test
APP_PIN_PANEL_LAYOUT_TEST_SRCS := tests/app_pin_panel_layout_test.c src/app/pins/app_pin_panel_layout.c
APP_PIN_PANEL_RENAME_TEST_TARGET := $(TEST_BIN_DIR)/app_pin_panel_rename_test
APP_PIN_PANEL_RENAME_TEST_SRCS := tests/app_pin_panel_rename_test.c src/app/pins/app_pin_name_edit.c
TILE_MANAGER_RESIDENCY_TEST_TARGET := $(TEST_BIN_DIR)/tile_manager_residency_test
TILE_MANAGER_RESIDENCY_TEST_SRCS := tests/tile_manager_residency_test.c src/map/tile_manager.c src/map/tile_source.c src/map/mft_loader.c src/core/log.c
APP_PINS_TEST_TARGET := $(TEST_BIN_DIR)/app_pins_test
APP_PINS_TEST_SRCS := src/app/history/history_gpx.c src/app/history/history_store.c src/app/history/history_commands.c src/app/history/history_status.c tests/app_pins_test.c src/app/pins/app_pins.c src/app/pins/app_pins_workspace.c src/app/pins/place_repository.c src/app/pins/place_commands.c src/app/pins/place_routes.c src/app/pins/place_maintenance.c src/app/pins/place_profile.c src/route/query.c src/route/route.c src/route/astar.c src/route/cost.c src/route/graph.c src/map/mercator.c src/map/geodesic.c src/core/log.c

ROUTE_COST_TEST_TARGET := $(TEST_BIN_DIR)/test_route_cost
ROUTE_COST_TEST_SRCS := tests/test_route_cost.c src/route/cost.c src/route/astar.c src/route/route.c src/route/graph.c src/map/geodesic.c src/map/mercator.c src/core/log.c

UTILITY_WORKSPACE_TEST_TARGET := $(TEST_BIN_DIR)/test_utility_workspace
UTILITY_WORKSPACE_TEST_SRCS := tests/test_utility_workspace.c src/app/app_utility_workspace.c
ROUTE_QUERY_TEST_TARGET := $(TEST_BIN_DIR)/test_route_query
ROUTE_QUERY_TEST_SRCS := tests/test_route_query.c src/route/query.c src/route/route.c src/route/astar.c src/route/cost.c src/route/graph.c src/map/mercator.c src/map/geodesic.c src/core/log.c

PLACE_TOOLS_TEST_TARGET := $(TEST_BIN_DIR)/test_place_tools
PLACE_TOOLS_TEST_SRCS := src/app/history/history_gpx.c src/app/history/history_store.c src/app/history/history_commands.c src/app/history/history_status.c tests/test_place_tools.c src/app/pins/app_place_tools.c src/app/pins/app_place_tools_draw.c src/app/pins/app_pins.c src/app/pins/app_pins_workspace.c src/app/pins/place_repository.c src/app/pins/place_commands.c src/app/pins/place_routes.c src/app/pins/place_maintenance.c src/app/pins/place_profile.c src/route/query.c src/route/route.c src/route/astar.c src/route/cost.c src/route/graph.c src/map/mercator.c src/map/geodesic.c src/core/log.c

REGION_BAND_SELECTION_TEST_TARGET := $(TEST_BIN_DIR)/test_region_band_selection
REGION_BAND_SELECTION_TEST_SRCS := tests/test_region_band_selection.c src/app/region.c src/map/tile_source.c

TILE_SUBMISSION_BUDGET_TEST_TARGET := $(TEST_BIN_DIR)/test_tile_submission_budget
TILE_SUBMISSION_BUDGET_TEST_SRCS := tests/test_tile_submission_budget.c src/app/app_tile_submission_budget.c
REGION_PREPARE_TEST_TARGET := $(TEST_BIN_DIR)/test_region_prepare
REGION_PREPARE_TEST_SRCS := src/app/app_region_transition.c src/core/time.c src/map/polygon_cache.c src/map/polygon_triangulator.c tests/test_region_prepare.c tests/tile_source_test_fixture.c src/app/region_prepare.c src/app/region.c src/app/region_loader.c src/app/app_view.c src/map/tile_manager.c src/map/tile_source.c src/map/mft_loader.c src/map/mercator.c src/map/tile_math.c src/camera/camera.c src/camera/camera_viewport_bridge.c src/core/log.c

HISTORY_UI_TEST_TARGET := $(TEST_BIN_DIR)/test_history_ui
HISTORY_UI_TEST_SRCS := src/app/history/history_views.c src/app/history/history_browser.c $(KIT_UI_DIR)/src/kit_ui.c $(KIT_RENDER_DIR)/src/kit_render.c tests/test_history_ui.c src/app/terrain_ui.c src/app/history/history_ui.c src/app/history_layout.c src/app/history/history_gpx.c src/app/history/history_commands.c src/app/history/history_status.c src/app/history/history_store.c $(filter-out tools/mapforge_places.c src/app/history/history_gpx.c src/app/history/history_commands.c src/app/history/history_status.c src/app/history/history_store.c,$(PLACES_SRCS))
