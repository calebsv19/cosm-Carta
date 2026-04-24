#include "core_units.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(double a, double b, double eps) {
    return fabs(a - b) <= eps;
}

int main(void) {
    CoreUnitKind kind = CORE_UNIT_UNKNOWN;
    CoreResult r;
    double value = 0.0;

    r = core_units_parse_kind("meters", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_METER) return 1;

    r = core_units_parse_kind("cm", &kind);
    if (r.code != CORE_OK || kind != CORE_UNIT_CENTIMETER) return 1;

    r = core_units_convert(100.0, CORE_UNIT_CENTIMETER, CORE_UNIT_METER, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 1.0, 1e-9)) return 1;

    r = core_units_convert(1.0, CORE_UNIT_FOOT, CORE_UNIT_INCH, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 12.0, 1e-9)) return 1;

    r = core_units_validate_world_scale(0.0);
    if (r.code == CORE_OK) return 1;

    r = core_units_unit_to_world(2.0, CORE_UNIT_METER, 0.5, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 4.0, 1e-9)) return 1;

    r = core_units_world_to_unit(4.0, 0.5, CORE_UNIT_METER, &value);
    if (r.code != CORE_OK || !nearly_equal(value, 2.0, 1e-9)) return 1;

    printf("core_units tests passed\n");
    return 0;
}
