#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

layout(push_constant) uniform PushConstants {
    mat4 u_Transform;
} pc;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Projection;
} camera;

layout(location = 0) out vec3 v_normal_ws;
layout(location = 1) out vec3 v_position_ws;

void main() {
    vec4 world_pos = pc.u_Transform * vec4(a_position, 1.0);
    gl_Position = camera.u_Projection * camera.u_View * world_pos;
    v_normal_ws = normalize(mat3(pc.u_Transform) * a_normal);
    v_position_ws = world_pos.xyz;
}
