/*
 * core_math.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_math.h"

#include <math.h>

CoreMathVec2f core_math_vec2_add(CoreMathVec2f a, CoreMathVec2f b) {
    CoreMathVec2f out = {a.x + b.x, a.y + b.y};
    return out;
}

CoreMathVec2f core_math_vec2_sub(CoreMathVec2f a, CoreMathVec2f b) {
    CoreMathVec2f out = {a.x - b.x, a.y - b.y};
    return out;
}

CoreMathVec2f core_math_vec2_scale(CoreMathVec2f v, float s) {
    CoreMathVec2f out = {v.x * s, v.y * s};
    return out;
}

float core_math_vec2_dot(CoreMathVec2f a, CoreMathVec2f b) {
    return (a.x * b.x) + (a.y * b.y);
}

float core_math_vec2_length(CoreMathVec2f v) {
    return sqrtf(core_math_vec2_dot(v, v));
}

CoreResult core_math_vec2_normalize(const CoreMathVec2f *in, CoreMathVec2f *out) {
    float len;

    if (!in || !out) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    len = core_math_vec2_length(*in);
    if (len <= 1e-20f) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "zero-length vector"};
        return r;
    }

    out->x = in->x / len;
    out->y = in->y / len;
    return core_result_ok();
}

CoreMathVec3f core_math_vec3_add(CoreMathVec3f a, CoreMathVec3f b) {
    CoreMathVec3f out = {a.x + b.x, a.y + b.y, a.z + b.z};
    return out;
}

CoreMathVec3f core_math_vec3_sub(CoreMathVec3f a, CoreMathVec3f b) {
    CoreMathVec3f out = {a.x - b.x, a.y - b.y, a.z - b.z};
    return out;
}

CoreMathVec3f core_math_vec3_scale(CoreMathVec3f v, float s) {
    CoreMathVec3f out = {v.x * s, v.y * s, v.z * s};
    return out;
}

float core_math_vec3_dot(CoreMathVec3f a, CoreMathVec3f b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

CoreMathVec3f core_math_vec3_cross(CoreMathVec3f a, CoreMathVec3f b) {
    CoreMathVec3f out = {
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)
    };
    return out;
}

float core_math_vec3_length(CoreMathVec3f v) {
    return sqrtf(core_math_vec3_dot(v, v));
}

CoreResult core_math_vec3_normalize(const CoreMathVec3f *in, CoreMathVec3f *out) {
    float len;

    if (!in || !out) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "invalid argument"};
        return r;
    }

    len = core_math_vec3_length(*in);
    if (len <= 1e-20f) {
        CoreResult r = {CORE_ERR_INVALID_ARG, "zero-length vector"};
        return r;
    }

    out->x = in->x / len;
    out->y = in->y / len;
    out->z = in->z / len;
    return core_result_ok();
}
