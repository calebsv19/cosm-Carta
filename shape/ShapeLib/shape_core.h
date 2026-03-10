#pragma once

#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShapeVec2 {
    float x;
    float y;
} ShapeVec2;

typedef enum {
    SHAPE_SEGMENT_LINE = 0,
    SHAPE_SEGMENT_CUBIC_BEZIER = 1
} ShapeSegmentType;

typedef struct {
    ShapeSegmentType type;
    ShapeVec2 p0;
    ShapeVec2 p1;
    ShapeVec2 c1;
    ShapeVec2 c2;
} ShapeSegment;

typedef struct {
    bool   closed;
    size_t segmentCount;
    ShapeSegment* segments;
} ShapePath;

typedef struct {
    const char* name;
    size_t      pathCount;
    ShapePath*  paths;
} Shape;

typedef struct {
    size_t shapeCount;
    Shape* shapes;
} ShapeDocument;

void ShapeDocument_Free(ShapeDocument* doc);

#ifdef __cplusplus
}
#endif
