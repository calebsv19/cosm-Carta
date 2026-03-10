#ifndef CORE_MATH_H
#define CORE_MATH_H

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoreMathVec2f {
    float x;
    float y;
} CoreMathVec2f;

typedef struct CoreMathVec3f {
    float x;
    float y;
    float z;
} CoreMathVec3f;

CoreMathVec2f core_math_vec2_add(CoreMathVec2f a, CoreMathVec2f b);
CoreMathVec2f core_math_vec2_sub(CoreMathVec2f a, CoreMathVec2f b);
CoreMathVec2f core_math_vec2_scale(CoreMathVec2f v, float s);
float core_math_vec2_dot(CoreMathVec2f a, CoreMathVec2f b);
float core_math_vec2_length(CoreMathVec2f v);
CoreResult core_math_vec2_normalize(const CoreMathVec2f *in, CoreMathVec2f *out);

CoreMathVec3f core_math_vec3_add(CoreMathVec3f a, CoreMathVec3f b);
CoreMathVec3f core_math_vec3_sub(CoreMathVec3f a, CoreMathVec3f b);
CoreMathVec3f core_math_vec3_scale(CoreMathVec3f v, float s);
float core_math_vec3_dot(CoreMathVec3f a, CoreMathVec3f b);
CoreMathVec3f core_math_vec3_cross(CoreMathVec3f a, CoreMathVec3f b);
float core_math_vec3_length(CoreMathVec3f v);
CoreResult core_math_vec3_normalize(const CoreMathVec3f *in, CoreMathVec3f *out);

#ifdef __cplusplus
}
#endif

#endif
