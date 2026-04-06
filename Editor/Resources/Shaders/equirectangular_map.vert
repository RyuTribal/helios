#version 460

layout (location = 0) in vec3 a_position;

layout(location = 0) out vec3 localPos;

layout(push_constant) uniform PushConstants {
    mat4 u_Projection;
    mat4 u_View;
} pc;

void main()
{
    localPos = a_position;
    gl_Position =  pc.u_Projection * pc.u_View * vec4(localPos, 1.0);
}
