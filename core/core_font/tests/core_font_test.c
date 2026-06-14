#include "core_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int assert_ok(CoreResult r) {
    return r.code == CORE_OK;
}

static int fake_path_exists(const char *path, void *user) {
    const char *needle = (const char *)user;
    return (needle && strstr(path, needle) != NULL) ? 1 : 0;
}

static int write_text_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "w");
    if (!fp) return 1;
    if (fputs(text, fp) == EOF) {
        fclose(fp);
        return 1;
    }
    if (fclose(fp) != 0) return 1;
    return 0;
}

static int write_capacity_manifest(const char *path) {
    FILE *fp = fopen(path, "w");
    size_t i;
    if (!fp) return 1;

    if (fputs("version=core_font_manifest_v1\n", fp) == EOF) {
        fclose(fp);
        return 1;
    }

    for (i = 0; i < CORE_FONT_MANIFEST_MAX_ENTRIES + 1u; ++i) {
        if (fprintf(fp,
                    "entry|preset=ide|role=ui_regular|family=Family%zu|style=Regular|weight=400|point=12|primary=primary%zu.ttf\n",
                    i,
                    i) < 0) {
            fclose(fp);
            return 1;
        }
    }

    return fclose(fp) == 0 ? 0 : 1;
}

static int write_oversized_manifest(const char *path) {
    FILE *fp = fopen(path, "w");
    size_t i;
    if (!fp) return 1;

    if (fputs("version=core_font_manifest_v1\nentry|preset=ide|role=ui_regular|family=", fp) == EOF) {
        fclose(fp);
        return 1;
    }
    for (i = 0; i < 4300u; ++i) {
        if (fputc('A', fp) == EOF) {
            fclose(fp);
            return 1;
        }
    }
    if (fputs("|style=Regular|weight=400|point=12|primary=font.ttf\n", fp) == EOF) {
        fclose(fp);
        return 1;
    }

    return fclose(fp) == 0 ? 0 : 1;
}

static int test_preset_and_role_coverage(void) {
    CoreFontPreset preset;
    CoreFontPreset preset_by_name;
    CoreFontRoleSpec role;
    CoreResult r;
    int tier_index;
    int preset_index;
    int role_index;

    for (preset_index = 0; preset_index < CORE_FONT_PRESET_COUNT; ++preset_index) {
        r = core_font_get_preset((CoreFontPresetId)preset_index, &preset);
        if (!assert_ok(r)) return 1;
        if (preset.id != (CoreFontPresetId)preset_index) return 1;
        if (!preset.name || !preset.name[0]) return 1;
        if (strcmp(core_font_preset_name((CoreFontPresetId)preset_index), preset.name) != 0) return 1;

        r = core_font_get_preset_by_name(preset.name, &preset_by_name);
        if (!assert_ok(r)) return 1;
        if (preset_by_name.id != preset.id) return 1;

        for (role_index = 0; role_index < CORE_FONT_ROLE_COUNT; ++role_index) {
            CoreFontRoleId role_id = (CoreFontRoleId)role_index;
            CoreFontRoleId roundtrip_role = CORE_FONT_ROLE_UI_REGULAR;
            const char *role_name = core_font_role_name(role_id);

            if (!role_name || !role_name[0]) return 1;
            r = core_font_role_id_from_name(role_name, &roundtrip_role);
            if (!assert_ok(r)) return 1;
            if (roundtrip_role != role_id) return 1;

            r = core_font_resolve_role(&preset, role_id, &role);
            if (!assert_ok(r)) return 1;
            if (role.role != role_id) return 1;
            if (!role.primary_path || !role.primary_path[0]) return 1;

            for (tier_index = 0; tier_index < CORE_FONT_TEXT_SIZE_COUNT; ++tier_index) {
                CoreFontTextSizeTier tier = (CoreFontTextSizeTier)tier_index;
                CoreFontTextSizeTier roundtrip_tier = CORE_FONT_TEXT_SIZE_BASIC;
                const char *tier_name = core_font_text_size_tier_name(tier);
                int sized = 0;

                if (!tier_name || !tier_name[0]) return 1;
                r = core_font_text_size_tier_from_name(tier_name, &roundtrip_tier);
                if (!assert_ok(r)) return 1;
                if (roundtrip_tier != tier) return 1;

                r = core_font_point_size_for_tier(&role, tier, &sized);
                if (!assert_ok(r)) return 1;
                if (sized < 6) return 1;
            }
        }
    }

    r = core_font_get_preset_by_name("missing", &preset);
    if (r.code == CORE_OK) return 1;

    return 0;
}

static int test_manifest_sample_and_unknown_key_tolerance(void) {
    CoreFontManifest manifest;
    CoreFontManifestEntry entry;
    CoreResult r;
    const char *unknown_key_manifest = "/tmp/core_font_manifest_unknown_key.txt";
    const char *text =
        "version=core_font_manifest_v1\n"
        "entry|preset=ide|role=ui_regular|family=Lato|style=Regular|weight=400|point=11|primary=fonts/Lato-Regular.ttf|future=allowed\n";

    r = core_font_manifest_parse_file("tests/fixtures/manifest_sample_v1.txt", &manifest);
    if (!assert_ok(r)) return 1;
    if (strcmp(manifest.version, "core_font_manifest_v1") != 0) return 1;
    if (manifest.entry_count != 3u) return 1;

    r = core_font_manifest_find_role(&manifest, "ide", CORE_FONT_ROLE_UI_MONO, &entry);
    if (!assert_ok(r)) return 1;
    if (strcmp(entry.fallback_path, "include/fonts/FiraCode/FiraCode-Regular.ttf") != 0) return 1;
    if (strcmp(entry.pack, "extended") != 0) return 1;

    if (write_text_file(unknown_key_manifest, text) != 0) return 1;
    r = core_font_manifest_parse_file(unknown_key_manifest, &manifest);
    if (!assert_ok(r)) return 1;
    if (manifest.entry_count != 1u) return 1;

    return 0;
}

static int test_manifest_malformed_inputs(void) {
    CoreFontManifest manifest;
    CoreResult r;
    const char *missing_version = "/tmp/core_font_missing_version.txt";
    const char *invalid_version = "/tmp/core_font_invalid_version.txt";
    const char *unknown_role = "/tmp/core_font_unknown_role.txt";
    const char *missing_required = "/tmp/core_font_missing_required.txt";
    const char *unknown_line = "/tmp/core_font_unknown_line.txt";
    const char *oversized_line = "/tmp/core_font_oversized_line.txt";
    const char *capacity_manifest = "/tmp/core_font_capacity_manifest.txt";

    if (write_text_file(missing_version,
                        "entry|preset=ide|role=ui_regular|family=Lato|style=Regular|weight=400|point=11|primary=fonts/Lato-Regular.ttf\n") != 0) {
        return 1;
    }
    r = core_font_manifest_parse_file(missing_version, &manifest);
    if (r.code == CORE_OK) return 1;

    if (write_text_file(invalid_version,
                        "version=core_font_manifest_v2\n"
                        "entry|preset=ide|role=ui_regular|family=Lato|style=Regular|weight=400|point=11|primary=fonts/Lato-Regular.ttf\n") != 0) {
        return 1;
    }
    r = core_font_manifest_parse_file(invalid_version, &manifest);
    if (r.code == CORE_OK) return 1;

    if (write_text_file(unknown_role,
                        "version=core_font_manifest_v1\n"
                        "entry|preset=ide|role=ui_missing|family=Lato|style=Regular|weight=400|point=11|primary=fonts/Lato-Regular.ttf\n") != 0) {
        return 1;
    }
    r = core_font_manifest_parse_file(unknown_role, &manifest);
    if (r.code == CORE_OK) return 1;

    if (write_text_file(missing_required,
                        "version=core_font_manifest_v1\n"
                        "entry|preset=ide|family=Lato|style=Regular|weight=400|point=11|primary=fonts/Lato-Regular.ttf\n") != 0) {
        return 1;
    }
    r = core_font_manifest_parse_file(missing_required, &manifest);
    if (r.code == CORE_OK) return 1;

    if (write_text_file(unknown_line,
                        "version=core_font_manifest_v1\n"
                        "surprise=this is not allowed\n") != 0) {
        return 1;
    }
    r = core_font_manifest_parse_file(unknown_line, &manifest);
    if (r.code == CORE_OK) return 1;

    if (write_oversized_manifest(oversized_line) != 0) return 1;
    r = core_font_manifest_parse_file(oversized_line, &manifest);
    if (r.code == CORE_OK) return 1;

    if (write_capacity_manifest(capacity_manifest) != 0) return 1;
    r = core_font_manifest_parse_file(capacity_manifest, &manifest);
    if (r.code == CORE_OK) return 1;

    return 0;
}

static int test_path_selection_contract(void) {
    CoreFontRoleSpec choose_role = {
        CORE_FONT_ROLE_UI_MONO,
        "Mono",
        "Regular",
        CORE_FONT_WEIGHT_NORMAL,
        12,
        "primary-ok.ttf",
        "fallback-ok.ttf"
    };
    CoreFontRoleSpec fallback_only = {
        CORE_FONT_ROLE_UI_MONO,
        "Mono",
        "Regular",
        CORE_FONT_WEIGHT_NORMAL,
        12,
        "",
        "fallback-ok.ttf"
    };
    CoreFontRoleSpec none = {
        CORE_FONT_ROLE_UI_MONO,
        "Mono",
        "Regular",
        CORE_FONT_WEIGHT_NORMAL,
        12,
        "",
        ""
    };
    const char *selected_path = NULL;
    CoreResult r;

    r = core_font_choose_path(&choose_role, NULL, NULL, &selected_path);
    if (!assert_ok(r)) return 1;
    if (strcmp(selected_path, "primary-ok.ttf") != 0) return 1;

    r = core_font_choose_path(&fallback_only, NULL, NULL, &selected_path);
    if (!assert_ok(r)) return 1;
    if (strcmp(selected_path, "fallback-ok.ttf") != 0) return 1;

    r = core_font_choose_path(&none, NULL, NULL, &selected_path);
    if (r.code == CORE_OK) return 1;

    r = core_font_choose_path(&choose_role, fake_path_exists, "missing", &selected_path);
    if (r.code == CORE_OK) return 1;

    r = core_font_choose_path(&choose_role, fake_path_exists, "primary-ok", &selected_path);
    if (!assert_ok(r)) return 1;
    if (strcmp(selected_path, "primary-ok.ttf") != 0) return 1;

    {
        CoreFontRoleSpec fallback_success = choose_role;
        fallback_success.primary_path = "missing-primary.ttf";
        fallback_success.fallback_path = "fallback-ok.ttf";
        r = core_font_choose_path(&fallback_success, fake_path_exists, "fallback-ok", &selected_path);
        if (!assert_ok(r)) return 1;
        if (strcmp(selected_path, "fallback-ok.ttf") != 0) return 1;
    }

    return 0;
}

int main(void) {
    if (test_preset_and_role_coverage() != 0) return 1;
    if (test_manifest_sample_and_unknown_key_tolerance() != 0) return 1;
    if (test_manifest_malformed_inputs() != 0) return 1;
    if (test_path_selection_contract() != 0) return 1;
    puts("core_font tests passed");
    return 0;
}
