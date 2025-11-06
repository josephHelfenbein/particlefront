#version 450

layout(location = 0) in vec2 texCoord;

struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

layout(binding = 0) uniform LightsBuffer {
    PointLight lights[64];
    uint numLights;
} lightsUBO;

layout(binding = 1) uniform sampler2D gBufferAlbedo;
layout(binding = 2) uniform sampler2D gBufferNormal;
layout(binding = 3) uniform sampler2D gBufferMaterial;
layout(binding = 4) uniform sampler2D gBufferDepth;

const float PI = 3.14159265358979323846;

layout(push_constant) uniform PushConstants {
    mat4 invView;
    mat4 invProj;
    vec3 cameraPos;
} pc;

layout(location = 0) out vec4 FragColor;

vec3 reconstructPosition(vec2 uv, float depth) {
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpace = pc.invProj * clipSpace;
    viewSpace /= viewSpace.w;
    vec4 worldSpace = pc.invView * viewSpace;
    return worldSpace.xyz;
}
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    cosTheta = clamp(cosTheta, 0.0, 1.0);
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float DistributionGGX(vec3 N, vec3 H, float roughness){
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}
float GeometrySchlickGGX(float NdotV, float roughness){
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0001);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness){
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    return ggx1 * ggx2;
}
float specularAntiAliasing(vec3 normal, float roughness){
    vec3 dndu = dFdx(normal);
    vec3 dndv = dFdy(normal);
    float variance = dot(dndu, dndu) + dot(dndv, dndv);
    float kernelRoughness = min(2.0 * variance, 1.0);
    return clamp(roughness + kernelRoughness, 0.0, 1.0);
}

void main() {
    vec3 albedo = texture(gBufferAlbedo, texCoord).rgb;
    float alpha = texture(gBufferAlbedo, texCoord).a;
    vec3 N = texture(gBufferNormal, texCoord).xyz * 2.0 - 1.0;
    vec4 material = texture(gBufferMaterial, texCoord);
    float metallic = material.r;
    float baseRoughness = material.g;

    float depth = texture(gBufferDepth, texCoord).r;
    if (depth >= 0.9999) {
        FragColor = vec4(albedo, 1.0);
        return;
    }
    
    vec3 fragPos = reconstructPosition(texCoord, depth);
    vec3 V = normalize(pc.cameraPos - fragPos);
    
    float roughness = specularAntiAliasing(N, baseRoughness);
    roughness = clamp(roughness, 0.05, 1.0);
    float dNdX = length(dFdx(N)),  dNdY = length(dFdy(N));
    float dVdX = length(dFdx(V)),  dVdY = length(dFdy(V));
    float sigma = max(max(dNdX, dNdY), max(dVdX, dVdY));
    roughness = max(roughness, sigma);

    vec3 totalLo = vec3(0.0);
    for (uint i = 0; i < lightsUBO.numLights; i++) {
        PointLight light = lightsUBO.lights[i];
        vec3 L = normalize(light.position - fragPos);
        vec3 H = normalize(V + L);
        float distance = length(light.position - fragPos);
        float attenuation = clamp(1.0 - (distance * distance) / (light.radius * light.radius), 0.0, 1.0);
        vec3 radiance = light.color * light.intensity * attenuation;

        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);

        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = fresnelSchlickRoughness(max(dot(V, H), 0.0), F0, roughness);
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);

        vec3 numerator = F * D * G;
        float denominator = 4.0 * max(NdotL * NdotV, 0.001);
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);
        vec4 diffuse = vec4(kD * albedo, alpha);

        totalLo += (diffuse.rgb + specular) * radiance * NdotL;
    }
    float alphaOut = max(max(totalLo.r, totalLo.g), max(totalLo.b, alpha));

    FragColor = vec4(totalLo, alphaOut);
}