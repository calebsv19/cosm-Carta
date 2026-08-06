#ifndef CORE_SCENE_VIEW_H
#define CORE_SCENE_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_SCENE_VIEW_SCHEMA_FAMILY "codework_scene_view"
#define CORE_SCENE_VIEW_PACKET_RAY_TRACING_VARIANT "ray_tracing_scene_view_packet_v0"

typedef enum CoreSceneViewPreviewQuality {
    CORE_SCENE_VIEW_PREVIEW_OUTLINE = 0,
    CORE_SCENE_VIEW_PREVIEW_FLAT_SOLID = 1,
    CORE_SCENE_VIEW_PREVIEW_MATERIAL = 2,
    CORE_SCENE_VIEW_PREVIEW_DIAGNOSTIC_OVERLAY = 3
} CoreSceneViewPreviewQuality;

typedef enum CoreSceneViewDegradedReason {
    CORE_SCENE_VIEW_DEGRADED_NONE = 0,
    CORE_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE = 1,
    CORE_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND = 2,
    CORE_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED = 3,
    CORE_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE = 4
} CoreSceneViewDegradedReason;

typedef enum CoreSceneViewDisplayFlags {
    CORE_SCENE_VIEW_DISPLAY_TRANSPARENT = 1u << 0,
    CORE_SCENE_VIEW_DISPLAY_EMISSIVE = 1u << 1,
    CORE_SCENE_VIEW_DISPLAY_MIRROR = 1u << 2,
    CORE_SCENE_VIEW_DISPLAY_TEXTURED = 1u << 3
} CoreSceneViewDisplayFlags;

typedef struct CoreSceneViewPickId {
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    int localTriangleIndex;
    int faceGroupIndex;
} CoreSceneViewPickId;

typedef struct CoreSceneViewPacketReadback {
    bool valid;
    int focusedObjectIndex;
    CoreSceneViewPreviewQuality previewQuality;
    CoreSceneViewDegradedReason degradedReason;
    bool projected;
    bool complete;
    int triangleCount;
    int faceGroupCount;
    CoreSceneViewPickId firstPickId;
    CoreSceneViewPickId lastPickId;
    uint32_t firstDisplayFlags;
    unsigned char firstAlpha;
} CoreSceneViewPacketReadback;

typedef struct CoreSceneViewPacketSummary {
    bool valid;
    bool readOnly;
    bool materialPreview;
    bool transparentPreview;
    bool emissivePreview;
    bool mirrorPreview;
    bool texturedPreview;
    bool projected;
    bool complete;
    int focusedObjectIndex;
    int triangleCount;
    int faceGroupCount;
    int firstFaceGroupIndex;
    int lastFaceGroupIndex;
    unsigned char firstAlpha;
    CoreSceneViewPreviewQuality previewQuality;
    CoreSceneViewDegradedReason degradedReason;
} CoreSceneViewPacketSummary;

const char *core_scene_view_preview_quality_name(CoreSceneViewPreviewQuality quality);
CoreResult core_scene_view_preview_quality_parse(const char *text,
                                                 CoreSceneViewPreviewQuality *out_quality);
const char *core_scene_view_degraded_reason_name(CoreSceneViewDegradedReason reason);
CoreResult core_scene_view_degraded_reason_parse(const char *text,
                                                 CoreSceneViewDegradedReason *out_reason);

void core_scene_view_packet_readback_init(CoreSceneViewPacketReadback *readback);
CoreResult core_scene_view_packet_readback_from_json_string(
    const char *json,
    CoreSceneViewPacketReadback *out_readback);
void core_scene_view_packet_summary_init(CoreSceneViewPacketSummary *summary);
CoreResult core_scene_view_packet_summary_from_readback(
    const CoreSceneViewPacketReadback *readback,
    CoreSceneViewPacketSummary *out_summary);

#ifdef __cplusplus
}
#endif

#endif
