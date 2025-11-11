#version 450

layout(location = 0) in vec3 vLightVector;
layout(location = 1) in float vLinearDepth;

layout(push_constant) uniform ShadowMapPushConstants {
    mat4 model;
    mat4 lightViewProj;
    vec4 lightPosFar;
} pc;

void main() {
    gl_FragDepth = vLinearDepth;
}