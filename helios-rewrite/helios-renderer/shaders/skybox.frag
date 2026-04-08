#version 460

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 local_pos;

layout(set = 0, binding = 0) uniform SkyboxUBO {
    mat4 u_CameraView;
    mat4 u_CameraProjection;
    float u_Brightness;
} ubo;

layout(set = 0, binding = 1) uniform samplerCube u_EnvironmentMap;

void main(){
    vec3 envColor = texture(u_EnvironmentMap, local_pos).rgb * ubo.u_Brightness;

    fragColor = vec4(envColor, 1.0);
}
