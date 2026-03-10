#ifndef CORE_FONT_H
#define CORE_FONT_H

#include "core_base.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_FONT_MANIFEST_MAX_ENTRIES 128
#define CORE_FONT_TEXT_MAX 256

typedef enum CoreFontPresetId {
    CORE_FONT_PRESET_DAW_DEFAULT = 0,
    CORE_FONT_PRESET_IDE = 1,
    CORE_FONT_PRESET_COUNT
} CoreFontPresetId;

typedef enum CoreFontRoleId {
    CORE_FONT_ROLE_UI_REGULAR = 0,
    CORE_FONT_ROLE_UI_MEDIUM = 1,
    CORE_FONT_ROLE_UI_BOLD = 2,
    CORE_FONT_ROLE_UI_MONO = 3,
    CORE_FONT_ROLE_UI_MONO_SMALL = 4,
    CORE_FONT_ROLE_COUNT
} CoreFontRoleId;

typedef enum CoreFontWeight {
    CORE_FONT_WEIGHT_NORMAL = 400,
    CORE_FONT_WEIGHT_MEDIUM = 500,
    CORE_FONT_WEIGHT_BOLD = 700
} CoreFontWeight;

typedef enum CoreFontTextSizeTier {
    CORE_FONT_TEXT_SIZE_BASIC = 0,
    CORE_FONT_TEXT_SIZE_PARAGRAPH = 1,
    CORE_FONT_TEXT_SIZE_TITLE = 2,
    CORE_FONT_TEXT_SIZE_HEADER = 3,
    CORE_FONT_TEXT_SIZE_CAPTION = 4,
    CORE_FONT_TEXT_SIZE_COUNT
} CoreFontTextSizeTier;

typedef struct CoreFontRoleSpec {
    CoreFontRoleId role;
    const char *family;
    const char *style;
    CoreFontWeight weight;
    int point_size;
    const char *primary_path;
    const char *fallback_path;
} CoreFontRoleSpec;

typedef struct CoreFontPreset {
    CoreFontPresetId id;
    const char *name;
    CoreFontRoleSpec roles[CORE_FONT_ROLE_COUNT];
} CoreFontPreset;

typedef struct CoreFontManifestEntry {
    char preset_name[64];
    CoreFontRoleId role;
    char family[64];
    char style[64];
    int weight;
    int point_size;
    char primary_path[CORE_FONT_TEXT_MAX];
    char fallback_path[CORE_FONT_TEXT_MAX];
    char license_id[64];
    char source[64];
    char pack[64];
} CoreFontManifestEntry;

typedef struct CoreFontManifest {
    char version[64];
    size_t entry_count;
    CoreFontManifestEntry entries[CORE_FONT_MANIFEST_MAX_ENTRIES];
} CoreFontManifest;

const char *core_font_preset_name(CoreFontPresetId id);
const char *core_font_role_name(CoreFontRoleId role);
const char *core_font_text_size_tier_name(CoreFontTextSizeTier tier);
CoreResult core_font_role_id_from_name(const char *name, CoreFontRoleId *out_role);
CoreResult core_font_text_size_tier_from_name(const char *name, CoreFontTextSizeTier *out_tier);
CoreResult core_font_get_preset(CoreFontPresetId id, CoreFontPreset *out_preset);
CoreResult core_font_get_preset_by_name(const char *name, CoreFontPreset *out_preset);
CoreResult core_font_resolve_role(const CoreFontPreset *preset,
                                  CoreFontRoleId role,
                                  CoreFontRoleSpec *out_role);
CoreResult core_font_point_size_for_tier(const CoreFontRoleSpec *role,
                                         CoreFontTextSizeTier tier,
                                         int *out_point_size);

void core_font_manifest_reset(CoreFontManifest *manifest);
CoreResult core_font_manifest_parse_file(const char *path, CoreFontManifest *out_manifest);
CoreResult core_font_manifest_find_role(const CoreFontManifest *manifest,
                                        const char *preset_name,
                                        CoreFontRoleId role,
                                        CoreFontManifestEntry *out_entry);

CoreResult core_font_choose_path(const CoreFontRoleSpec *role,
                                 int (*path_exists_fn)(const char *path, void *user),
                                 void *user,
                                 const char **out_path);

#ifdef __cplusplus
}
#endif

#endif
