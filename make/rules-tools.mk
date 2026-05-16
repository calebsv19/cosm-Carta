tools:
	tools/run_with_progress.sh --label "make tools" $(MAKE) --no-print-directory tools-build

tools-build: $(TOOL_TARGET) $(REGION_VALIDATE_TARGET)

$(TOOL_TARGET): $(TOOL_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(TOOL_SRCS) -o $@ $(TOOL_LDLIBS)

$(REGION_VALIDATE_TARGET): $(REGION_VALIDATE_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(REGION_VALIDATE_SRCS) -o $@ $(TOOL_LDLIBS) $(if $(JSON_LIBS),$(JSON_LIBS),-ljson-c)

graph:
	tools/run_with_progress.sh --label "make graph" $(MAKE) --no-print-directory graph-build

graph-build: $(GRAPH_TARGET)

$(GRAPH_TARGET): $(GRAPH_SRCS) $(CORE_IO_LIB) $(CORE_DATA_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude $(GRAPH_SRCS) -o $@ $(TOOL_LDLIBS)

