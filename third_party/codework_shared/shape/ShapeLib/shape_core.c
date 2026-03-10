#include "ShapeLib/shape_core.h"
#include <stdlib.h>

void ShapeDocument_Free(ShapeDocument* doc) {
    if (!doc) return;
    if (doc->shapes) {
        for (size_t si = 0; si < doc->shapeCount; ++si) {
            Shape* s = &doc->shapes[si];
            if (!s->paths) continue;
            for (size_t pi = 0; pi < s->pathCount; ++pi) {
                ShapePath* p = &s->paths[pi];
                free(p->segments);
                p->segments = NULL;
                p->segmentCount = 0;
            }
            free(s->paths);
            s->paths = NULL;
            s->pathCount = 0;
        }
        free(doc->shapes);
        doc->shapes = NULL;
    }
    doc->shapeCount = 0;
}
