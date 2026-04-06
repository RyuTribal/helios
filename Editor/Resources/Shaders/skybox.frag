#version 460

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 local_pos;

layout(set = 1, binding = 1) uniform samplerCube u_EnvironmentMap;

layout(push_constant) uniform PushConstants {
    float u_Brightness;
} pc;

void main(){
    vec3 envColor = texture(u_EnvironmentMap, local_pos).rgb * pc.u_Brightness;

    // tone mapping is handled in hdr shader

    fragColor = vec4(envColor, 1.0);
}
