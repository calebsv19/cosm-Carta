CC := cc

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SDL_TTF_CFLAGS := $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell pkg-config --libs SDL2_ttf 2>/dev/null)

ifeq ($(SDL_LIBS),)
SDL_CFLAGS :=
SDL_LIBS := -lSDL2
endif

ifeq ($(SDL_TTF_LIBS),)
SDL_TTF_LIBS := -lSDL2_ttf
endif

CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2 -g -pthread $(SDL_CFLAGS) $(SDL_TTF_CFLAGS)
LDLIBS := $(SDL_LIBS) $(SDL_TTF_LIBS) -pthread
TOOL_LDLIBS := -lm

SRCS := $(shell find src -name '*.c')
OBJS := $(SRCS:src/%.c=build/%.o)
TARGET := build/mapforge
TOOL_TARGET := build/tools/mapforge_region
TOOL_SRCS := tools/mapforge_region.c src/map/mercator.c src/map/tile_math.c src/core/log.c
GRAPH_TARGET := build/tools/mapforge_graph
GRAPH_SRCS := tools/mapforge_graph.c src/map/mercator.c src/core/log.c

MIN_Z ?= 12
MAX_Z ?= 12

app: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

run: app
	./$(TARGET)

tools: $(TOOL_TARGET)

$(TOOL_TARGET): $(TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(TOOL_SRCS) -o $@ $(TOOL_LDLIBS)

graph: $(GRAPH_TARGET)

$(GRAPH_TARGET): $(GRAPH_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(GRAPH_SRCS) -o $@ $(TOOL_LDLIBS)

route: graph
	./$(GRAPH_TARGET) --region $(REGION) --osm $(OSM) --out data/regions/$(REGION)

region: tools
	./$(TOOL_TARGET) --region $(REGION) --osm $(OSM) --out data/regions/$(REGION) --min-z $(MIN_Z) --max-z $(MAX_Z)

clean:
	rm -rf build

.PHONY: app run tools graph route region clean
