/*
 * core_units.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_units.h"

#include <ctype.h>
#include <string.h>

typedef struct CoreUnitInfo {
    CoreUnitKind kind;
    const char *name;
    const char *symbol;
    double meters_per_unit;
} CoreUnitInfo;

static const CoreUnitInfo k_units[] = {
    { CORE_UNIT_METER, "meters", "m", 1.0 },
    { CORE_UNIT_CENTIMETER, "centimeters", "cm", 0.01 },
    { CORE_UNIT_MILLIMETER, "millimeters", "mm", 0.001 },
    { CORE_UNIT_INCH, "inches", "in", 0.0254 },
    { CORE_UNIT_FOOT, "feet", "ft", 0.3048 }
};

static const CoreUnitInfo *core_units_info(CoreUnitKind kind) {
    size_t i;
    for (i = 0; i < sizeof(k_units) / sizeof(k_units[0]); ++i) {
        if (k_units[i].kind == kind) {
            return &k_units[i];
        }
    }
    return NULL;
}

static int text_equals_ci(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }
    while (*a && *b) {
        if ((char)tolower((unsigned char)*a) != (char)tolower((unsigned char)*b)) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

const char *core_units_kind_name(CoreUnitKind kind) {
    const CoreUnitInfo *info = core_units_info(kind);
    return info ? info->name : "unknown";
}

const char *core_units_kind_symbol(CoreUnitKind kind) {
    const CoreUnitInfo *info = core_units_info(kind);
    return info ? info->symbol : "?";
}

CoreResult core_units_parse_kind(const char *text, CoreUnitKind *out_kind) {
    size_t i;
    if (!text || !out_kind) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    for (i = 0; i < sizeof(k_units) / sizeof(k_units[0]); ++i) {
        const CoreUnitInfo *info = &k_units[i];
        if (text_equals_ci(text, info->name) || text_equals_ci(text, info->symbol)) {
            *out_kind = info->kind;
            return core_result_ok();
        }
    }

    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown unit kind" };
        return r;
    }
}

CoreResult core_units_convert(double value,
                              CoreUnitKind from_kind,
                              CoreUnitKind to_kind,
                              double *out_value) {
    const CoreUnitInfo *from_info = core_units_info(from_kind);
    const CoreUnitInfo *to_info = core_units_info(to_kind);
    if (!out_value) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "out_value is null" };
        return r;
    }
    if (!from_info || !to_info) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "unknown unit kind" };
        return r;
    }

    {
        double meters = value * from_info->meters_per_unit;
        *out_value = meters / to_info->meters_per_unit;
    }
    return core_result_ok();
}

CoreResult core_units_validate_world_scale(double world_scale) {
    if (world_scale <= 0.0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "world_scale must be > 0" };
        return r;
    }
    return core_result_ok();
}

CoreResult core_units_unit_to_world(double value,
                                    CoreUnitKind from_kind,
                                    double world_scale,
                                    double *out_world_value) {
    double meters = 0.0;
    CoreResult scale_r = core_units_validate_world_scale(world_scale);
    if (!out_world_value) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "out_world_value is null" };
        return r;
    }
    if (scale_r.code != CORE_OK) {
        return scale_r;
    }

    {
        CoreResult r = core_units_convert(value, from_kind, CORE_UNIT_METER, &meters);
        if (r.code != CORE_OK) {
            return r;
        }
    }

    *out_world_value = meters / world_scale;
    return core_result_ok();
}

CoreResult core_units_world_to_unit(double world_value,
                                    double world_scale,
                                    CoreUnitKind to_kind,
                                    double *out_value) {
    double meters = 0.0;
    CoreResult scale_r = core_units_validate_world_scale(world_scale);
    if (!out_value) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "out_value is null" };
        return r;
    }
    if (scale_r.code != CORE_OK) {
        return scale_r;
    }

    meters = world_value * world_scale;
    return core_units_convert(meters, CORE_UNIT_METER, to_kind, out_value);
}
