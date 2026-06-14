#include "core_math.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

int main(void) {
    CoreMathVec2f a2 = {1.0f, 2.0f};
    CoreMathVec2f b2 = {3.0f, 4.0f};
    CoreMathVec2f out2 = {99.0f, 88.0f};
    CoreMathVec3f a3 = {1.0f, 0.0f, 0.0f};
    CoreMathVec3f b3 = {0.0f, 1.0f, 0.0f};
    CoreMathVec3f out3 = {77.0f, 66.0f, 55.0f};
    CoreResult r;

    out2 = core_math_vec2_add(a2, b2);
    if (!nearly_equal(out2.x, 4.0f, 1e-6f) || !nearly_equal(out2.y, 6.0f, 1e-6f)) return 1;

    out2 = core_math_vec2_sub(b2, a2);
    if (!nearly_equal(out2.x, 2.0f, 1e-6f) || !nearly_equal(out2.y, 2.0f, 1e-6f)) return 1;

    out2 = core_math_vec2_scale(a2, 2.5f);
    if (!nearly_equal(out2.x, 2.5f, 1e-6f) || !nearly_equal(out2.y, 5.0f, 1e-6f)) return 1;

    if (!nearly_equal(core_math_vec2_dot(a2, b2), 11.0f, 1e-6f)) return 1;
    if (!nearly_equal(core_math_vec2_length((CoreMathVec2f){3.0f, 4.0f}), 5.0f, 1e-6f)) return 1;

    r = core_math_vec2_normalize(&(CoreMathVec2f){3.0f, 4.0f}, &out2);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(out2.x, 0.6f, 1e-6f) || !nearly_equal(out2.y, 0.8f, 1e-6f)) return 1;

    out2 = (CoreMathVec2f){9.0f, 8.0f};
    r = core_math_vec2_normalize(NULL, &out2);
    if (r.code == CORE_OK) return 1;
    if (!nearly_equal(out2.x, 9.0f, 1e-6f) || !nearly_equal(out2.y, 8.0f, 1e-6f)) return 1;

    r = core_math_vec2_normalize(&(CoreMathVec2f){3.0f, 4.0f}, NULL);
    if (r.code == CORE_OK) return 1;

    out2 = (CoreMathVec2f){7.0f, 6.0f};
    r = core_math_vec2_normalize(&(CoreMathVec2f){0.0f, 0.0f}, &out2);
    if (r.code == CORE_OK) return 1;
    if (!nearly_equal(out2.x, 7.0f, 1e-6f) || !nearly_equal(out2.y, 6.0f, 1e-6f)) return 1;

    out2 = (CoreMathVec2f){5.0f, 4.0f};
    r = core_math_vec2_normalize(&(CoreMathVec2f){1e-30f, 0.0f}, &out2);
    if (r.code == CORE_OK) return 1;
    if (!nearly_equal(out2.x, 5.0f, 1e-6f) || !nearly_equal(out2.y, 4.0f, 1e-6f)) return 1;

    out2 = (CoreMathVec2f){3.0f, 2.0f};
    r = core_math_vec2_normalize(&(CoreMathVec2f){INFINITY, 1.0f}, &out2);
    if (r.code == CORE_OK) return 1;
    if (!nearly_equal(out2.x, 3.0f, 1e-6f) || !nearly_equal(out2.y, 2.0f, 1e-6f)) return 1;

    out3 = core_math_vec3_add(a3, b3);
    if (!nearly_equal(out3.x, 1.0f, 1e-6f) || !nearly_equal(out3.y, 1.0f, 1e-6f) || !nearly_equal(out3.z, 0.0f, 1e-6f)) return 1;

    out3 = core_math_vec3_sub((CoreMathVec3f){5.0f, 4.0f, 3.0f}, (CoreMathVec3f){1.0f, 2.0f, 3.0f});
    if (!nearly_equal(out3.x, 4.0f, 1e-6f) || !nearly_equal(out3.y, 2.0f, 1e-6f) || !nearly_equal(out3.z, 0.0f, 1e-6f)) return 1;

    out3 = core_math_vec3_scale((CoreMathVec3f){1.0f, -2.0f, 3.0f}, -2.0f);
    if (!nearly_equal(out3.x, -2.0f, 1e-6f) || !nearly_equal(out3.y, 4.0f, 1e-6f) || !nearly_equal(out3.z, -6.0f, 1e-6f)) return 1;

    out3 = core_math_vec3_cross(a3, b3);
    if (!nearly_equal(out3.x, 0.0f, 1e-6f) || !nearly_equal(out3.y, 0.0f, 1e-6f) || !nearly_equal(out3.z, 1.0f, 1e-6f)) return 1;

    out3 = core_math_vec3_cross(b3, a3);
    if (!nearly_equal(out3.x, 0.0f, 1e-6f) || !nearly_equal(out3.y, 0.0f, 1e-6f) || !nearly_equal(out3.z, -1.0f, 1e-6f)) return 1;

    if (!nearly_equal(core_math_vec3_dot(a3, b3), 0.0f, 1e-6f)) return 1;

    r = core_math_vec3_normalize(&(CoreMathVec3f){0.0f, 0.0f, 5.0f}, &out3);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(out3.x, 0.0f, 1e-6f) || !nearly_equal(out3.y, 0.0f, 1e-6f) || !nearly_equal(out3.z, 1.0f, 1e-6f)) return 1;

    out3 = (CoreMathVec3f){9.0f, 8.0f, 7.0f};
    r = core_math_vec3_normalize(NULL, &out3);
    if (r.code == CORE_OK) return 1;
    if (!nearly_equal(out3.x, 9.0f, 1e-6f) || !nearly_equal(out3.y, 8.0f, 1e-6f) || !nearly_equal(out3.z, 7.0f, 1e-6f)) return 1;

    r = core_math_vec3_normalize(&(CoreMathVec3f){0.0f, 0.0f, 5.0f}, NULL);
    if (r.code == CORE_OK) return 1;

    out3 = (CoreMathVec3f){6.0f, 5.0f, 4.0f};
    r = core_math_vec3_normalize(&(CoreMathVec3f){1e-30f, 0.0f, 0.0f}, &out3);
    if (r.code == CORE_OK) return 1;
    if (!nearly_equal(out3.x, 6.0f, 1e-6f) || !nearly_equal(out3.y, 5.0f, 1e-6f) || !nearly_equal(out3.z, 4.0f, 1e-6f)) return 1;

    out3 = (CoreMathVec3f){3.0f, 2.0f, 1.0f};
    r = core_math_vec3_normalize(&(CoreMathVec3f){NAN, 0.0f, 1.0f}, &out3);
    if (r.code == CORE_OK) return 1;
    if (!nearly_equal(out3.x, 3.0f, 1e-6f) || !nearly_equal(out3.y, 2.0f, 1e-6f) || !nearly_equal(out3.z, 1.0f, 1e-6f)) return 1;

    puts("core_math tests passed");
    return 0;
}
