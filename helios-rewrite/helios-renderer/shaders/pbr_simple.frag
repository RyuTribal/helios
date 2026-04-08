#version 460

layout(location = 0) in vec3 v_position_ws;
layout(location = 1) in vec3 v_normal_ws;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in mat3 v_TBN;  // locations 3,4,5

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Projection;
    vec3 u_CameraPos;
    float _pad0;
    vec3 u_LightDir;
    float _pad1;
    vec3 u_LightColor;
    float u_LightIntensity;
} camera;

layout(set = 1, binding = 0) uniform sampler2D u_Albedo;
layout(set = 1, binding = 1) uniform sampler2D u_NormalMap;
layout(set = 1, binding = 2) uniform sampler2D u_MetallicRoughness;
layout(set = 1, binding = 3) uniform sampler2D u_Emissive;
layout(set = 1, binding = 4) uniform samplerCube u_EnvMap;

layout(location = 0) out vec4 fragColor;

const float PI = 3.14159265359;

// GGX/Trowbridge-Reitz normal distribution
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

// Smith's Schlick-GGX geometry function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // Sample textures
    vec4 albedo_sample = texture(u_Albedo, v_uv);
    vec3 albedo = pow(albedo_sample.rgb, vec3(2.2)); // sRGB to linear

    vec3 normal_map = texture(u_NormalMap, v_uv).rgb * 2.0 - 1.0;
    vec3 N = normalize(v_TBN * normal_map);

    vec4 mr_sample = texture(u_MetallicRoughness, v_uv);
    float roughness = mr_sample.g;
    float metallic  = mr_sample.b;

    vec3 emissive = pow(texture(u_Emissive, v_uv).rgb, vec3(2.2));

    // Vectors
    vec3 V = normalize(camera.u_CameraPos - v_position_ws);
    vec3 L = normalize(-camera.u_LightDir);
    vec3 H = normalize(V + L);
    vec3 R = reflect(-V, N);

    // F0 for dielectrics ~0.04, metals use albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Cook-Torrance BRDF for directional light
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * camera.u_LightColor * camera.u_LightIntensity * NdotL;

    // Naive environment reflections (IBL approximation)
    float max_lod = 4.0; // cubemap has few mips, keep it simple
    vec3 env_specular = textureLod(u_EnvMap, R, roughness * max_lod).rgb;
    vec3 env_fresnel  = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 env_reflect  = env_specular * env_fresnel * (1.0 - roughness * 0.5);

    // Ambient: rough cubemap sample at normal direction
    vec3 env_diffuse = textureLod(u_EnvMap, N, max_lod).rgb;
    vec3 ambient     = kD * albedo * env_diffuse * 0.3;

    vec3 color = Lo + ambient + env_reflect * 0.5 + emissive;

    fragColor = vec4(color, 1.0);
}
