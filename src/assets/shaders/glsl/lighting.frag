#version 450

layout(location = 0) in vec2 texCoord;

struct PointLight {
    vec4 positionRadius;
    vec4 colorIntensity;
    mat4 lightViewProj[6];
    vec4 shadowParams;
    uvec4 shadowData;
};

layout(binding = 0) uniform LightsBuffer {
    PointLight lights[64];
    uvec4 lightCounts;
} lightsUBO;

layout(binding = 1) uniform sampler2D gBufferAlbedo;
layout(binding = 2) uniform sampler2D gBufferNormal;
layout(binding = 3) uniform sampler2D gBufferMaterial;
layout(binding = 4) uniform sampler2D gBufferDepth;
layout(binding = 5) uniform samplerCube shadowMaps[64];

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

const uint INVALID_SHADOW_INDEX = 0xffffffffu;

const vec3 sampleOffsetDirections[20] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float computePointShadow(PointLight light, vec3 fragPos, vec3 geomNormal, vec3 lightDir) {
    if (light.shadowData.y == 0u) {
        return 1.0;
    }
    uint shadowIndex = light.shadowData.x;
    if (shadowIndex == INVALID_SHADOW_INDEX || shadowIndex >= 64u) {
        return 1.0;
    }

    vec3 lightPos = light.positionRadius.xyz;
    vec3 toFrag = fragPos - lightPos;
    float currentDistance = length(toFrag);
    if (currentDistance <= 0.0001) {
        return 1.0;
    }

    float farPlane = light.shadowParams.z;
    if (currentDistance >= farPlane) {
        return 1.0;
    }

    vec3 sampleDir = normalize(toFrag);
    
    float depthSample = texture(shadowMaps[shadowIndex], sampleDir).r;
    
    if (depthSample >= 0.9999) {
        return 1.0;
    }
    uint samples = 30u;
    float viewDistance = length(pc.cameraPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / farPlane)) / 25.0;
    float shadow = 0.0;
    for (uint i = 0u; i < samples; i++) {
        vec3 offsetDir = sampleOffsetDirections[i];
        vec3 sampleVec = sampleDir + offsetDir * diskRadius;
        sampleVec = normalize(sampleVec);
        float depthSample = texture(shadowMaps[shadowIndex], sampleVec).r;
        float closestDistance = depthSample * farPlane;
        
        float NoLGeom = max(dot(geomNormal, lightDir), 0.0);
        float baseBias = light.shadowParams.x;
        float normalBias = baseBias * 5.0 * (1.0 - NoLGeom);
        float bias = baseBias + normalBias;
        
        if (currentDistance > closestDistance + bias) {
            shadow += 1.0;
        }
    }
    shadow /= float(samples);
    float strength = clamp(light.shadowParams.w, 0.0, 1.0);
    return 1.0 - shadow * strength;
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
    vec3 geomNormal = cross(dFdx(fragPos), dFdy(fragPos));
    if (dot(geomNormal, geomNormal) > 1e-10) {
        geomNormal = normalize(geomNormal);
    } else {
        geomNormal = N;
    }
    if (dot(geomNormal, N) < 0.0) {
        geomNormal = -geomNormal;
    }
    
    float roughness = specularAntiAliasing(N, baseRoughness);
    roughness = clamp(roughness, 0.05, 1.0);
    float dNdX = length(dFdx(N)),  dNdY = length(dFdy(N));
    float dVdX = length(dFdx(V)),  dVdY = length(dFdy(V));
    float sigma = max(max(dNdX, dNdY), max(dVdX, dVdY));
    roughness = max(roughness, sigma);

    uint numLights = lightsUBO.lightCounts.x;

    vec3 totalLo = vec3(0.0);
    for (uint i = 0u; i < numLights; i++) {
        PointLight light = lightsUBO.lights[i];
        vec3 lightPos = light.positionRadius.xyz;
        float radius = light.positionRadius.w;
        vec3 lightColor = light.colorIntensity.rgb;
        float intensity = light.colorIntensity.w;

        vec3 L = normalize(lightPos - fragPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPos - fragPos);
        float attenuation = clamp(1.0 - (distance * distance) / (radius * radius), 0.0, 1.0);
        vec3 radiance = lightColor * intensity * attenuation;

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

        float shadowVisibility = computePointShadow(light, fragPos, geomNormal, L);

        vec3 lightingContribution = (diffuse.rgb + specular) * radiance * NdotL;
        totalLo += lightingContribution * shadowVisibility;
    }
    float alphaOut = max(max(totalLo.r, totalLo.g), max(totalLo.b, alpha));

    FragColor = vec4(totalLo, alphaOut);
}