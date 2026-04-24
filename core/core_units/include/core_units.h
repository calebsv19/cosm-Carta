#ifndef CORE_UNITS_H
#define CORE_UNITS_H

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CoreUnitKind {
    CORE_UNIT_UNKNOWN = 0,
    CORE_UNIT_METER = 1,
    CORE_UNIT_CENTIMETER = 2,
    CORE_UNIT_MILLIMETER = 3,
    CORE_UNIT_INCH = 4,
    CORE_UNIT_FOOT = 5
} CoreUnitKind;

const char *core_units_kind_name(CoreUnitKind kind);
const char *core_units_kind_symbol(CoreUnitKind kind);
CoreResult core_units_parse_kind(const char *text, CoreUnitKind *out_kind);
CoreResult core_units_convert(double value,
                              CoreUnitKind from_kind,
                              CoreUnitKind to_kind,
                              double *out_value);
CoreResult core_units_validate_world_scale(double world_scale);
CoreResult core_units_unit_to_world(double value,
                                    CoreUnitKind from_kind,
                                    double world_scale,
                                    double *out_world_value);
CoreResult core_units_world_to_unit(double world_value,
                                    double world_scale,
                                    CoreUnitKind to_kind,
                                    double *out_value);

#ifdef __cplusplus
}
#endif

#endif
