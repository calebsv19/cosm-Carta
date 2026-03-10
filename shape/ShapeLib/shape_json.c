#include "ShapeLib/shape_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include("external/cjson/cJSON.h")
#include "external/cjson/cJSON.h"
#elif __has_include("../external/cjson/cJSON.h")
#include "../external/cjson/cJSON.h"
#elif __has_include("render/TimerHUD/external/cJSON.h")
#include "render/TimerHUD/external/cJSON.h"
#else
#error "cJSON header not found for shape_json"
#endif

static cJSON* ShapeVec2_ToJson(ShapeVec2 v) {
    cJSON* arr = cJSON_CreateArray();
    if (!arr) return NULL;
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(v.x));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(v.y));
    return arr;
}

static bool ShapeVec2_FromJson(cJSON* arr, ShapeVec2* out) {
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) != 2 || !out) return false;
    cJSON* xi = cJSON_GetArrayItem(arr, 0);
    cJSON* yi = cJSON_GetArrayItem(arr, 1);
    if (!cJSON_IsNumber(xi) || !cJSON_IsNumber(yi)) return false;
    out->x = (float)xi->valuedouble;
    out->y = (float)yi->valuedouble;
    return true;
}

static char* ReadWholeFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) { free(buf); return NULL; }
    buf[sz] = '\0';
    return buf;
}

bool ShapeDocument_SaveToJsonFile(const ShapeDocument* doc,
                                  const char* path) {
    if (!doc || !path) return false;

    cJSON* root = cJSON_CreateObject();
    if (!root) return false;
    cJSON_AddNumberToObject(root, "version", 1);

    cJSON* shapesArr = cJSON_AddArrayToObject(root, "shapes");
    if (!shapesArr) { cJSON_Delete(root); return false; }

    for (size_t si = 0; si < doc->shapeCount; ++si) {
        const Shape* s = &doc->shapes[si];
        cJSON* sObj = cJSON_CreateObject();
        if (!sObj) { cJSON_Delete(root); return false; }
        cJSON_AddItemToArray(shapesArr, sObj);

        if (s->name) {
            cJSON_AddStringToObject(sObj, "name", s->name);
        }

        cJSON* pathsArr = cJSON_AddArrayToObject(sObj, "paths");
        if (!pathsArr) { cJSON_Delete(root); return false; }

        for (size_t pi = 0; pi < s->pathCount; ++pi) {
            const ShapePath* p = &s->paths[pi];
            cJSON* pObj = cJSON_CreateObject();
            if (!pObj) { cJSON_Delete(root); return false; }
            cJSON_AddItemToArray(pathsArr, pObj);

            cJSON_AddBoolToObject(pObj, "closed", p->closed);

            cJSON* segArr = cJSON_AddArrayToObject(pObj, "segments");
            if (!segArr) { cJSON_Delete(root); return false; }

            for (size_t segi = 0; segi < p->segmentCount; ++segi) {
                const ShapeSegment* seg = &p->segments[segi];
                cJSON* segObj = cJSON_CreateObject();
                if (!segObj) { cJSON_Delete(root); return false; }
                cJSON_AddItemToArray(segArr, segObj);

                const char* t = (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) ? "cubic" : "line";
                cJSON_AddStringToObject(segObj, "type", t);
                cJSON_AddItemToObject(segObj, "p0", ShapeVec2_ToJson(seg->p0));
                cJSON_AddItemToObject(segObj, "p1", ShapeVec2_ToJson(seg->p1));
                if (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) {
                    cJSON_AddItemToObject(segObj, "c1", ShapeVec2_ToJson(seg->c1));
                    cJSON_AddItemToObject(segObj, "c2", ShapeVec2_ToJson(seg->c2));
                }
            }
        }
    }

    char* text = cJSON_Print(root);
    if (!text) { cJSON_Delete(root); return false; }

    FILE* f = fopen(path, "wb");
    if (!f) { cJSON_Delete(root); free(text); return false; }
    size_t len = strlen(text);
    size_t n = fwrite(text, 1, len, f);
    fclose(f);
    cJSON_Delete(root);
    free(text);
    return n == len;
}

bool ShapeDocument_LoadFromJsonFile(const char* path,
                                    ShapeDocument* outDoc) {
    if (!path || !outDoc) return false;
    memset(outDoc, 0, sizeof(*outDoc));

    char* text = ReadWholeFile(path);
    if (!text) return false;

    cJSON* root = cJSON_Parse(text);
    free(text);
    if (!root) return false;

    cJSON* verItem = cJSON_GetObjectItem(root, "version");
    int version = cJSON_IsNumber(verItem) ? verItem->valueint : 1;
    if (version != 1) { cJSON_Delete(root); return false; }

    cJSON* shapesArr = cJSON_GetObjectItem(root, "shapes");
    if (!cJSON_IsArray(shapesArr)) { cJSON_Delete(root); return false; }

    int shapeCount = cJSON_GetArraySize(shapesArr);
    if (shapeCount < 0) { cJSON_Delete(root); return false; }

    if (shapeCount == 0) {
        outDoc->shapeCount = 0;
        outDoc->shapes = NULL;
        cJSON_Delete(root);
        return true;
    }

    Shape* shapes = (Shape*)calloc((size_t)shapeCount, sizeof(Shape));
    if (!shapes) { cJSON_Delete(root); return false; }
    outDoc->shapeCount = (size_t)shapeCount;
    outDoc->shapes = shapes;

    for (int si = 0; si < shapeCount; ++si) {
        cJSON* sObj = cJSON_GetArrayItem(shapesArr, si);
        if (!cJSON_IsObject(sObj)) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }

        Shape* s = &shapes[si];

        cJSON* nameItem = cJSON_GetObjectItem(sObj, "name");
        if (cJSON_IsString(nameItem) && nameItem->valuestring) {
            size_t len = strlen(nameItem->valuestring);
            char* copy = (char*)malloc(len + 1);
            if (!copy) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }
            memcpy(copy, nameItem->valuestring, len + 1);
            s->name = copy;
        }

        cJSON* pathsArr = cJSON_GetObjectItem(sObj, "paths");
        if (!cJSON_IsArray(pathsArr)) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }

        int pathCount = cJSON_GetArraySize(pathsArr);
        if (pathCount < 0) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }

        if (pathCount > 0) {
            s->paths = (ShapePath*)calloc((size_t)pathCount, sizeof(ShapePath));
            if (!s->paths) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }
            s->pathCount = (size_t)pathCount;
        }

        for (int pi = 0; pi < pathCount; ++pi) {
            cJSON* pObj = cJSON_GetArrayItem(pathsArr, pi);
            if (!cJSON_IsObject(pObj)) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }

            ShapePath* p = &s->paths[pi];
            cJSON* closedItem = cJSON_GetObjectItem(pObj, "closed");
            p->closed = cJSON_IsTrue(closedItem);

            cJSON* segArr = cJSON_GetObjectItem(pObj, "segments");
            if (!cJSON_IsArray(segArr)) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }

            int segCount = cJSON_GetArraySize(segArr);
            if (segCount < 0) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }

            if (segCount > 0) {
                p->segments = (ShapeSegment*)calloc((size_t)segCount, sizeof(ShapeSegment));
                if (!p->segments) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }
                p->segmentCount = (size_t)segCount;
            }

            for (int segi = 0; segi < segCount; ++segi) {
                cJSON* segObj = cJSON_GetArrayItem(segArr, segi);
                if (!cJSON_IsObject(segObj)) { ShapeDocument_Free(outDoc); cJSON_Delete(root); return false; }

                ShapeSegment* seg = &p->segments[segi];
                cJSON* typeItem = cJSON_GetObjectItem(segObj, "type");
                if (!cJSON_IsString(typeItem) || !typeItem->valuestring) {
                    ShapeDocument_Free(outDoc); cJSON_Delete(root); return false;
                }
                if (strcmp(typeItem->valuestring, "cubic") == 0) {
                    seg->type = SHAPE_SEGMENT_CUBIC_BEZIER;
                } else {
                    seg->type = SHAPE_SEGMENT_LINE;
                }

                cJSON* p0Item = cJSON_GetObjectItem(segObj, "p0");
                cJSON* p1Item = cJSON_GetObjectItem(segObj, "p1");
                if (!ShapeVec2_FromJson(p0Item, &seg->p0) ||
                    !ShapeVec2_FromJson(p1Item, &seg->p1)) {
                    ShapeDocument_Free(outDoc); cJSON_Delete(root); return false;
                }

                if (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) {
                    cJSON* c1Item = cJSON_GetObjectItem(segObj, "c1");
                    cJSON* c2Item = cJSON_GetObjectItem(segObj, "c2");
                    if (!ShapeVec2_FromJson(c1Item, &seg->c1) ||
                        !ShapeVec2_FromJson(c2Item, &seg->c2)) {
                        ShapeDocument_Free(outDoc); cJSON_Delete(root); return false;
                    }
                }
            }
        }
    }

    cJSON_Delete(root);
    return true;
}
