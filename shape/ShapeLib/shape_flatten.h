#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "ShapeLib/shape_core.h"

typedef struct {
    ShapeVec2* points;
    size_t     count;
    bool       closed;
} Polyline;

typedef struct {
    Polyline* lines;
    size_t    count;
} PolylineSet;

bool Shape_FlattenToPolylines(const Shape* shape,
                              float maxError,
                              PolylineSet* out);

void PolylineSet_Free(PolylineSet* set);
