#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

layout(push_constant) uniform ShadowMapPushConstants {
    mat4 model;
    mat4 lightViewProj;
    vec4 lightPosFar;
} pc;

layout(location = 0) out vec3 vLightVector;
layout(location = 1) out float vLinearDepth;

void main() {
    vec4 worldPos = pc.model * vec4(aPos, 1.0);
    vLightVector = worldPos.xyz - pc.lightPosFar.xyz;
    
    float distance = length(vLightVector);
    vLinearDepth = clamp(distance / pc.lightPosFar.w, 0.0, 1.0);
    
    gl_Position = pc.lightViewProj * worldPos;
}
