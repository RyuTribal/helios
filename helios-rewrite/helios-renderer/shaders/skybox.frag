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
    // Negate Y to correct for Vulkan's negative-viewport-height Y flip
    vec3 sample_dir = vec3(local_pos.x, -local_pos.y, local_pos.z);
    vec3 envColor = texture(u_EnvironmentMap, sample_dir).rgb * ubo.u_Brightness;

    fragColor = vec4(envColor, 1.0);
}
