#include "core_units.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearly_equal(double a, double b, double eps) {
    return fabs(a - b) <= eps;
}

int main(void) {
    CoreUnitKind kind = CORE_UNIT_UNKNOWN;
    CoreResult r;
    double value = 0.0;

    if (strcmp(core_units_kind_name(CORE_UNIT_METER), "meter") != 0) return 1;
    if (strcmp(core_units_kind_name(CORE_UNIT_CENTIMETER), "centimeter") != 0) return 1;
    if (strcmp(core_units_kind_name(CORE_UNIT_MILLIMETER), "millimeter") != 0) return 1;
    if (strcmp(core_units_kind_name(CORE_UNIT_INCH), "inch") != 0) return 1;
    if (strcmp(core_units_kind_name(CORE_UNIT_FOOT), "foot") != 0) return 1;
    if (strcmp(core_units_kind_name(CORE_UNIT_UNKNOWN), "unknown") != 0) return 1;

    if (strcmp(core_units_kind_symbol(CORE_UNIT_METER), "m") != 0) return 1;
    if (strcmp(core_units_kind_symbol(CORE_UNIT_CENTIMETER), "cm") != 0) return 1;
    if (strcmp(core_units_kind_symbol(CORE_UNIT_MILLIMETER), "mm") != 0) return 1;
    if (strcmp(core_units_kind_symbol(CORE_UNIT_INCH), "in") != 0) return 1;
    if (strcmp(core_units_kind_symbol(CORE_UNIT_FOOT), "ft") != 0) return 1;
    if (strcmp(core_units_kind_symbol(CORE_UNIT_UNKNOWN), "?") != 0) return 1;

    r = core_units_parse_kind("meter", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_METER) return 1;
    r = core_units_parse_kind("meters", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_METER) return 1;
    r = core_units_parse_kind("M", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_METER) return 1;
    r = core_units_parse_kind("centimeter", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_CENTIMETER) return 1;
    r = core_units_parse_kind("CENTIMETERS", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_CENTIMETER) return 1;
    r = core_units_parse_kind("Cm", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_CENTIMETER) return 1;
    r = core_units_parse_kind("millimeter", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_MILLIMETER) return 1;
    r = core_units_parse_kind("millimeters", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_MILLIMETER) return 1;
    r = core_units_parse_kind("MM", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_MILLIMETER) return 1;
    r = core_units_parse_kind("inch", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_INCH) return 1;
    r = core_units_parse_kind("inches", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_INCH) return 1;
    r = core_units_parse_kind("IN", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_INCH) return 1;
    r = core_units_parse_kind("foot", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_FOOT) return 1;
    r = core_units_parse_kind("feet", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_FOOT) return 1;
    r = core_units_parse_kind("Ft", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_FOOT) return 1;

    kind = CORE_UNIT_FOOT;
    r = core_units_parse_kind("yards", &kind);
    if (r.code != CORE_ERR_NOT_FOUND || kind != CORE_UNIT_UNKNOWN) return 1;

    kind = CORE_UNIT_FOOT;
    r = core_units_parse_kind(NULL, &kind);
    if (r.code != CORE_ERR_INVALID_ARG || kind != CORE_UNIT_UNKNOWN) return 1;
    r = core_units_parse_kind("meters", NULL);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    r = core_units_convert(100.0, CORE_UNIT_CENTIMETER, CORE_UNIT_METER, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 1.0, 1e-9)) return 1;

    r = core_units_convert(1.0, CORE_UNIT_FOOT, CORE_UNIT_INCH, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 12.0, 1e-9)) return 1;

    r = core_units_convert(-12.0, CORE_UNIT_INCH, CORE_UNIT_FOOT, &value);
    if (r.code != CORE_OK || !nearly_equal(value, -1.0, 1e-9)) return 1;

    value = 77.0;
    r = core_units_convert(NAN, CORE_UNIT_METER, CORE_UNIT_FOOT, &value);
    if (r.code != CORE_ERR_INVALID_ARG || !nearly_equal(value, 0.0, 1e-12)) return 1;

    value = 77.0;
    r = core_units_convert(1.0, CORE_UNIT_UNKNOWN, CORE_UNIT_FOOT, &value);
    if (r.code != CORE_ERR_INVALID_ARG || !nearly_equal(value, 0.0, 1e-12)) return 1;

    value = 77.0;
    r = core_units_convert(1.0, CORE_UNIT_METER, CORE_UNIT_UNKNOWN, &value);
    if (r.code != CORE_ERR_INVALID_ARG || !nearly_equal(value, 0.0, 1e-12)) return 1;

    r = core_units_convert(1.0, CORE_UNIT_METER, CORE_UNIT_FOOT, NULL);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    r = core_units_validate_world_scale(0.0);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    r = core_units_validate_world_scale(-1.0);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    r = core_units_validate_world_scale(NAN);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    r = core_units_validate_world_scale(INFINITY);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    r = core_units_validate_world_scale(0.5);
    if (r.code != CORE_OK) return 1;

    r = core_units_unit_to_world(2.0, CORE_UNIT_METER, 0.5, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 4.0, 1e-9)) return 1;

    r = core_units_world_to_unit(4.0, 0.5, CORE_UNIT_METER, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 2.0, 1e-9)) return 1;

    r = core_units_unit_to_world(-24.0, CORE_UNIT_INCH, 0.5, &value);
    if (r.code != CORE_OK || !nearly_equal(value, -1.2192, 1e-9)) return 1;

    r = core_units_world_to_unit(-1.2192, 0.5, CORE_UNIT_INCH, &value);
    if (r.code != CORE_OK || !nearly_equal(value, -24.0, 1e-9)) return 1;

    value = 7.0;
    r = core_units_unit_to_world(1.0, CORE_UNIT_METER, NAN, &value);
    if (r.code != CORE_ERR_INVALID_ARG || !nearly_equal(value, 0.0, 1e-12)) return 1;

    value = 7.0;
    r = core_units_world_to_unit(1.0, INFINITY, CORE_UNIT_METER, &value);
    if (r.code != CORE_ERR_INVALID_ARG || !nearly_equal(value, 0.0, 1e-12)) return 1;

    r = core_units_unit_to_world(1.0, CORE_UNIT_METER, 1.0, NULL);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    r = core_units_world_to_unit(1.0, 1.0, CORE_UNIT_METER, NULL);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    printf("core_units tests passed\n");
    return 0;
}
