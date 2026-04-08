#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;

layout(push_constant) uniform PushConstants {
    mat4 u_Transform;
} pc;

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

layout(location = 0) out vec3 v_position_ws;
layout(location = 1) out vec3 v_normal_ws;
layout(location = 2) out vec2 v_uv;
layout(location = 3) out mat3 v_TBN;  // locations 3,4,5

void main() {
    vec4 world_pos = pc.u_Transform * vec4(a_position, 1.0);
    gl_Position = camera.u_Projection * camera.u_View * world_pos;
    v_position_ws = world_pos.xyz;

    mat3 normal_matrix = mat3(pc.u_Transform);
    vec3 N = normalize(normal_matrix * a_normal);
    vec3 T = normalize(normal_matrix * a_tangent.xyz);
    T = normalize(T - dot(T, N) * N);  // re-orthogonalize
    vec3 B = cross(N, T) * a_tangent.w;
    v_TBN = mat3(T, B, N);

    v_normal_ws = N;
    v_uv = a_uv;
}
