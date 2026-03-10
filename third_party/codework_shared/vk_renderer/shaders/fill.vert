#version 450

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
    vec2 mode;
    vec4 tint;
    vec4 transform0;
    vec4 transform1;
} pc;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fsColor;

void main() {
    vec2 pos = inPosition;
    if (pc.mode.x > 0.5) {
        vec3 p = vec3(inPosition, 1.0);
        pos = vec2(dot(pc.transform0.xyz, p), dot(pc.transform1.xyz, p));
    }
    vec2 ndc = ((pos / pc.screenSize) * 2.0) - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    fsColor = inColor * pc.tint;
}
