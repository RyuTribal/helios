#version 460

layout (location = 0) in vec3 a_coords;

layout(push_constant) uniform PushConstants {
    mat4 u_Transform;
} pc;


void main(){
    gl_Position = pc.u_Transform * vec4(a_coords, 1.0);
}
