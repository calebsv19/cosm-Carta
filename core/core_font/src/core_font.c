/*
 * core_font.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_font.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *k_manifest_version = "core_font_manifest_v1";
static const int k_text_size_percent[CORE_FONT_TEXT_SIZE_COUNT] = {100, 108, 120, 132, 90};

static const CoreFontPreset k_font_presets[CORE_FONT_PRESET_COUNT] = {
    {
        CORE_FONT_PRESET_DAW_DEFAULT,
        "daw_default",
        {
            {
                CORE_FONT_ROLE_UI_REGULAR,
                "Montserrat",
                "Regular",
                CORE_FONT_WEIGHT_NORMAL,
                9,
                "include/fonts/Montserrat/Montserrat-Regular.ttf",
                "shared/assets/fonts/Montserrat-Regular.ttf"
            },
            {
                CORE_FONT_ROLE_UI_MEDIUM,
                "Montserrat",
                "Medium",
                CORE_FONT_WEIGHT_MEDIUM,
                9,
                "include/fonts/Montserrat/Montserrat-Medium.ttf",
                "shared/assets/fonts/Montserrat-Medium.ttf"
            },
            {
                CORE_FONT_ROLE_UI_BOLD,
                "Montserrat",
                "Bold",
                CORE_FONT_WEIGHT_BOLD,
                9,
                "include/fonts/Montserrat/Montserrat-Bold.ttf",
                "shared/assets/fonts/Montserrat-Bold.ttf"
            },
            {
                CORE_FONT_ROLE_UI_MONO,
                "Lato",
                "Regular",
                CORE_FONT_WEIGHT_NORMAL,
                13,
                "include/fonts/Lato/Lato-Regular.ttf",
                "shared/assets/fonts/Lato-Regular.ttf"
            },
            {
                CORE_FONT_ROLE_UI_MONO_SMALL,
                "Lato",
                "Regular",
                CORE_FONT_WEIGHT_NORMAL,
                12,
                "include/fonts/Lato/Lato-Regular.ttf",
                "shared/assets/fonts/Lato-Regular.ttf"
            }
        }
    },
    {
        CORE_FONT_PRESET_IDE,
        "ide",
        {
            {
                CORE_FONT_ROLE_UI_REGULAR,
                "Lato",
                "Regular",
                CORE_FONT_WEIGHT_NORMAL,
                11,
                "include/fonts/Lato/Lato-Regular.ttf",
                "shared/assets/fonts/Lato-Regular.ttf"
            },
            {
                CORE_FONT_ROLE_UI_MEDIUM,
                "Lato",
                "Regular",
                CORE_FONT_WEIGHT_NORMAL,
                11,
                "include/fonts/Lato/Lato-Regular.ttf",
                "shared/assets/fonts/Lato-Regular.ttf"
            },
            {
                CORE_FONT_ROLE_UI_BOLD,
                "Lato",
                "Bold",
                CORE_FONT_WEIGHT_BOLD,
                11,
                "include/fonts/Lato/Lato-Bold.ttf",
                "shared/assets/fonts/Lato-Bold.ttf"
            },
            {
                CORE_FONT_ROLE_UI_MONO,
                "Lato",
                "Regular",
                CORE_FONT_WEIGHT_NORMAL,
                11,
                "include/fonts/Lato/Lato-Regular.ttf",
                "shared/assets/fonts/Lato-Regular.ttf"
            },
            {
                CORE_FONT_ROLE_UI_MONO_SMALL,
                "Lato",
                "Regular",
                CORE_FONT_WEIGHT_NORMAL,
                10,
                "include/fonts/Lato/Lato-Regular.ttf",
                "shared/assets/fonts/Lato-Regular.ttf"
            }
        }
    }
};

static void copy_text(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static char *trim(char *text) {
    char *end;
    while (*text && isspace((unsigned char)*text)) {
        text++;
    }
    if (!*text) {
        return text;
    }
    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return text;
}

static CoreResult parse_manifest_entry_line(char *line, CoreFontManifestEntry *out_entry) {
    char *segment;
    char *cursor;
    int has_preset = 0;
    int has_role = 0;

    if (!line || !out_entry) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    *out_entry = (CoreFontManifestEntry){0};
    cursor = line;

    segment = strtok(cursor, "|");
    if (!segment || strcmp(segment, "entry") != 0) {
        CoreResult r = {CORE_ERR_FORMAT, "invalid entry prefix"};
        return r;
    }

    while ((segment = strtok(NULL, "|")) != NULL) {
        char *eq = strchr(segment, '=');
        char *key;
        char *value;
        CoreFontRoleId role;

        if (!eq) {
            CoreResult r = {CORE_ERR_FORMAT, "invalid key/value segment"};
            return r;
        }
        *eq = '\0';
        key = trim(segment);
        value = trim(eq + 1);

        if (strcmp(key, "preset") == 0) {
            copy_text(out_entry->preset_name, sizeof(out_entry->preset_name), value);
            has_preset = 1;
        } else if (strcmp(key, "role") == 0) {
            CoreResult rr = core_font_role_id_from_name(value, &role);
            if (rr.code != CORE_OK) {
                CoreResult r = {CORE_ERR_FORMAT, "unknown role"};
                return r;
            }
            out_entry->role = role;
            has_role = 1;
        } else if (strcmp(key, "family") == 0) {
            copy_text(out_entry->family, sizeof(out_entry->family), value);
        } else if (strcmp(key, "style") == 0) {
            copy_text(out_entry->style, sizeof(out_entry->style), value);
        } else if (strcmp(key, "weight") == 0) {
            out_entry->weight = atoi(value);
        } else if (strcmp(key, "point") == 0) {
            out_entry->point_size = atoi(value);
        } else if (strcmp(key, "primary") == 0) {
            copy_text(out_entry->primary_path, sizeof(out_entry->primary_path), value);
        } else if (strcmp(key, "fallback") == 0) {
            copy_text(out_entry->fallback_path, sizeof(out_entry->fallback_path), value);
        } else if (strcmp(key, "license") == 0) {
            copy_text(out_entry->license_id, sizeof(out_entry->license_id), value);
        } else if (strcmp(key, "source") == 0) {
            copy_text(out_entry->source, sizeof(out_entry->source), value);
        } else if (strcmp(key, "pack") == 0) {
            copy_text(out_entry->pack, sizeof(out_entry->pack), value);
        }
    }

    if (!has_preset || !has_role || out_entry->primary_path[0] == '\0') {
        CoreResult r = {CORE_ERR_FORMAT, "missing required entry fields"};
        return r;
    }

    if (out_entry->weight == 0) {
        out_entry->weight = CORE_FONT_WEIGHT_NORMAL;
    }
    if (out_entry->point_size <= 0) {
        out_entry->point_size = 12;
    }

    return core_result_ok();
}

const char *core_font_preset_name(CoreFontPresetId id) {
    if ((int)id < 0 || id >= CORE_FONT_PRESET_COUNT) {
        return "";
    }
    return k_font_presets[id].name;
}

const char *core_font_role_name(CoreFontRoleId role) {
    switch (role) {
        case CORE_FONT_ROLE_UI_REGULAR:
            return "ui_regular";
        case CORE_FONT_ROLE_UI_MEDIUM:
            return "ui_medium";
        case CORE_FONT_ROLE_UI_BOLD:
            return "ui_bold";
        case CORE_FONT_ROLE_UI_MONO:
            return "ui_mono";
        case CORE_FONT_ROLE_UI_MONO_SMALL:
            return "ui_mono_small";
        default:
            return "";
    }
}

const char *core_font_text_size_tier_name(CoreFontTextSizeTier tier) {
    switch (tier) {
        case CORE_FONT_TEXT_SIZE_BASIC:
            return "basic";
        case CORE_FONT_TEXT_SIZE_PARAGRAPH:
            return "paragraph";
        case CORE_FONT_TEXT_SIZE_TITLE:
            return "title";
        case CORE_FONT_TEXT_SIZE_HEADER:
            return "header";
        case CORE_FONT_TEXT_SIZE_CAPTION:
            return "caption";
        default:
            return "";
    }
}

CoreResult core_font_role_id_from_name(const char *name, CoreFontRoleId *out_role) {
    if (!name || !name[0] || !out_role) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    if (strcmp(name, "ui_regular") == 0) {
        *out_role = CORE_FONT_ROLE_UI_REGULAR;
    } else if (strcmp(name, "ui_medium") == 0) {
        *out_role = CORE_FONT_ROLE_UI_MEDIUM;
    } else if (strcmp(name, "ui_bold") == 0) {
        *out_role = CORE_FONT_ROLE_UI_BOLD;
    } else if (strcmp(name, "ui_mono") == 0) {
        *out_role = CORE_FONT_ROLE_UI_MONO;
    } else if (strcmp(name, "ui_mono_small") == 0) {
        *out_role = CORE_FONT_ROLE_UI_MONO_SMALL;
    } else {
        CoreResult r = {CORE_ERR_NOT_FOUND, "role not found"};
        return r;
    }

    return core_result_ok();
}

CoreResult core_font_text_size_tier_from_name(const char *name, CoreFontTextSizeTier *out_tier) {
    if (!name || !name[0] || !out_tier) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    if (strcmp(name, "basic") == 0) {
        *out_tier = CORE_FONT_TEXT_SIZE_BASIC;
    } else if (strcmp(name, "paragraph") == 0) {
        *out_tier = CORE_FONT_TEXT_SIZE_PARAGRAPH;
    } else if (strcmp(name, "title") == 0) {
        *out_tier = CORE_FONT_TEXT_SIZE_TITLE;
    } else if (strcmp(name, "header") == 0) {
        *out_tier = CORE_FONT_TEXT_SIZE_HEADER;
    } else if (strcmp(name, "caption") == 0) {
        *out_tier = CORE_FONT_TEXT_SIZE_CAPTION;
    } else {
        CoreResult r = {CORE_ERR_NOT_FOUND, "size tier not found"};
        return r;
    }

    return core_result_ok();
}

CoreResult core_font_get_preset(CoreFontPresetId id, CoreFontPreset *out_preset) {
    if (!out_preset) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "out_preset is null"};
        return r;
    }
    if ((int)id < 0 || id >= CORE_FONT_PRESET_COUNT) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid preset id"};
        return r;
    }
    *out_preset = k_font_presets[id];
    return core_result_ok();
}

CoreResult core_font_get_preset_by_name(const char *name, CoreFontPreset *out_preset) {
    int i;
    if (!name || !name[0]) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "name is empty"};
        return r;
    }
    if (!out_preset) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "out_preset is null"};
        return r;
    }
    for (i = 0; i < CORE_FONT_PRESET_COUNT; ++i) {
        if (strcmp(name, k_font_presets[i].name) == 0) {
            *out_preset = k_font_presets[i];
            return core_result_ok();
        }
    }
    {
        CoreResult r = {CORE_ERR_NOT_FOUND, "preset not found"};
        return r;
    }
}

CoreResult core_font_resolve_role(const CoreFontPreset *preset,
                                  CoreFontRoleId role,
                                  CoreFontRoleSpec *out_role) {
    int i;
    if (!preset || !out_role) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }
    for (i = 0; i < CORE_FONT_ROLE_COUNT; ++i) {
        if (preset->roles[i].role == role) {
            *out_role = preset->roles[i];
            return core_result_ok();
        }
    }
    {
        CoreResult r = {CORE_ERR_NOT_FOUND, "role not found"};
        return r;
    }
}

CoreResult core_font_point_size_for_tier(const CoreFontRoleSpec *role,
                                         CoreFontTextSizeTier tier,
                                         int *out_point_size) {
    int base;
    int pct;
    int scaled;

    if (!role || !out_point_size) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }
    if ((int)tier < 0 || tier >= CORE_FONT_TEXT_SIZE_COUNT) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid size tier"};
        return r;
    }

    base = role->point_size > 0 ? role->point_size : 12;
    pct = k_text_size_percent[tier];
    scaled = (base * pct + 50) / 100;
    if (scaled < 6) {
        scaled = 6;
    }

    *out_point_size = scaled;
    return core_result_ok();
}

void core_font_manifest_reset(CoreFontManifest *manifest) {
    if (!manifest) {
        return;
    }
    *manifest = (CoreFontManifest){0};
    copy_text(manifest->version, sizeof(manifest->version), k_manifest_version);
}

CoreResult core_font_manifest_parse_file(const char *path, CoreFontManifest *out_manifest) {
    FILE *fp;
    char line[2048];
    int saw_version = 0;

    if (!path || !path[0] || !out_manifest) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    fp = fopen(path, "r");
    if (!fp) {
        CoreResult r = {CORE_ERR_IO, "failed to open manifest"};
        return r;
    }

    core_font_manifest_reset(out_manifest);

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *entry_line = trim(line);

        if (!entry_line[0] || entry_line[0] == '#') {
            continue;
        }

        if (strncmp(entry_line, "version=", 8) == 0) {
            char *value = trim(entry_line + 8);
            copy_text(out_manifest->version, sizeof(out_manifest->version), value);
            saw_version = 1;
            continue;
        }

        if (strncmp(entry_line, "entry|", 6) == 0) {
            CoreFontManifestEntry parsed;
            CoreResult r;
            if (out_manifest->entry_count >= CORE_FONT_MANIFEST_MAX_ENTRIES) {
                fclose(fp);
                {
                    CoreResult cap = {CORE_ERR_OUT_OF_MEMORY, "manifest capacity reached"};
                    return cap;
                }
            }

            r = parse_manifest_entry_line(entry_line, &parsed);
            if (r.code != CORE_OK) {
                fclose(fp);
                return r;
            }

            out_manifest->entries[out_manifest->entry_count++] = parsed;
            continue;
        }

        fclose(fp);
        {
            CoreResult r = {CORE_ERR_FORMAT, "unknown manifest line"};
            return r;
        }
    }

    fclose(fp);

    if (!saw_version || strcmp(out_manifest->version, k_manifest_version) != 0) {
        CoreResult r = {CORE_ERR_FORMAT, "invalid manifest version"};
        return r;
    }

    return core_result_ok();
}

CoreResult core_font_manifest_find_role(const CoreFontManifest *manifest,
                                        const char *preset_name,
                                        CoreFontRoleId role,
                                        CoreFontManifestEntry *out_entry) {
    size_t i;

    if (!manifest || !preset_name || !preset_name[0] || !out_entry) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    for (i = 0; i < manifest->entry_count; ++i) {
        const CoreFontManifestEntry *entry = &manifest->entries[i];
        if (entry->role == role && strcmp(entry->preset_name, preset_name) == 0) {
            *out_entry = *entry;
            return core_result_ok();
        }
    }

    {
        CoreResult r = {CORE_ERR_NOT_FOUND, "manifest role entry not found"};
        return r;
    }
}

CoreResult core_font_choose_path(const CoreFontRoleSpec *role,
                                 int (*path_exists_fn)(const char *path, void *user),
                                 void *user,
                                 const char **out_path) {
    int primary_ok;
    int fallback_ok;

    if (!role || !out_path) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    if (!path_exists_fn) {
        if (role->primary_path && role->primary_path[0]) {
            *out_path = role->primary_path;
            return core_result_ok();
        }
        if (role->fallback_path && role->fallback_path[0]) {
            *out_path = role->fallback_path;
            return core_result_ok();
        }
        {
            CoreResult r = {CORE_ERR_NOT_FOUND, "no path candidates available"};
            return r;
        }
    }

    primary_ok = (role->primary_path && role->primary_path[0])
        ? path_exists_fn(role->primary_path, user)
        : 0;
    if (primary_ok) {
        *out_path = role->primary_path;
        return core_result_ok();
    }

    fallback_ok = (role->fallback_path && role->fallback_path[0])
        ? path_exists_fn(role->fallback_path, user)
        : 0;
    if (fallback_ok) {
        *out_path = role->fallback_path;
        return core_result_ok();
    }

    {
        CoreResult r = {CORE_ERR_NOT_FOUND, "no existing font path found"};
        return r;
    }
}
