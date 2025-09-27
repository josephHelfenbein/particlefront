#version 450
layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 normalVec;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 camPos;

const float PI = 3.14159265358979323846;

layout(binding = 1) uniform sampler2D albedoMap;
layout(binding = 2) uniform sampler2D metallicMap;
layout(binding = 3) uniform sampler2D roughnessMap;
layout(binding = 4) uniform sampler2D normalMap;

layout(location = 0) out vec4 FragColor;

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
vec3 getNormalFromMap(){
    vec3 tangent = texture(normalMap, texCoord).xyz * 2.0 - 1.0;
    vec3 Q1 = dFdx(FragPos);
    vec3 Q2 = dFdy(FragPos);
    vec2 st1 = dFdx(texCoord);
    vec2 st2 = dFdy(texCoord);
    vec3 N = normalize(normalVec);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangent);
}
float specularAntiAliasing(vec3 normal, float roughness){
    vec3 dndu = dFdx(normal);
    vec3 dndv = dFdy(normal);
    float variance = dot(dndu, dndu) + dot(dndv, dndv);
    float kernelRoughness = min(2.0 * variance, 1.0);
    return clamp(roughness + kernelRoughness, 0.0, 1.0);
}
void main(){
    vec4 baseColor = texture(albedoMap, texCoord);
    vec3 albedo = pow(baseColor.rgb, vec3(2.2));
    float alpha = baseColor.a;

    float metallic = texture(metallicMap, texCoord).r;

    float mask = texture(metallicMap, texCoord).a;
    if (mask < 0.2) discard;

    vec3 N = getNormalFromMap();
    vec3 V = normalize(camPos - FragPos);
    float baseRoughness = max(texture(roughnessMap, texCoord).r, 0.05);
    float roughness = specularAntiAliasing(N, baseRoughness);
    roughness = clamp(roughness, 0.05, 1.0);
    float dNdX = length(dFdx(N)),  dNdY = length(dFdy(N));
    float dVdX = length(dFdx(V)),  dVdY = length(dFdy(V));
    float sigma = max(max(dNdX, dNdY), max(dVdX, dVdY));
    roughness = max(roughness, sigma);

    vec3 L = normalize(vec3(-1.0, -0.5, 1.0));
    vec3 H = normalize(V + L);
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

    vec3 radiance = NdotL * vec3(3.0);


    vec3 L2 = normalize(vec3(1.0, -0.5, 1.0));
    vec3 H2 = normalize(V + L2);
    float NdotL2 = max(dot(N, L2), 0.0);
    float NdotV2 = max(dot(N, V), 0.0);

    vec3 F2 = fresnelSchlickRoughness(max(dot(V, H2), 0.0), F0, roughness);
    float D2 = DistributionGGX(N, H2, roughness);
    float G2 = GeometrySmith(N, V, L2, roughness);

    vec3 numerator2 = F2 * D2 * G2;
    float denominator2 = 4.0 * max(NdotL2 * NdotV2, 0.001);
    vec3 specular2 = numerator2 / denominator2;

    vec3 kS2 = F2;
    vec3 kD2 = (1.0 - kS2) * (1.0 - metallic);
    vec4 diffuse2 = vec4(kD2 * albedo, alpha);

    vec3 radiance2 = NdotL2 * vec3(3.0);

    vec3 Lo = (diffuse.rgb + specular) * radiance + (diffuse2.rgb + specular2) * radiance2;
    float alphaOut = max(max(Lo.r, Lo.g), max(Lo.b, diffuse.a));

    FragColor = vec4(Lo, alphaOut);
}