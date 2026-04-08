#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec4 a_transform_row1;
layout(location = 3) in vec4 a_transform_row2;
layout(location = 4) in vec4 a_transform_row3;
layout(location = 5) in vec4 a_transform_row4;

layout(push_constant) uniform PushConstants {
    mat4 u_CameraProjection;
    mat4 u_CameraView;
} pc;

layout(location = 0) out vec4 vColor;

void main()
{
   mat4 instanceTransform = mat4(a_transform_row1, a_transform_row2, a_transform_row3, a_transform_row4);
   gl_Position = pc.u_CameraProjection * pc.u_CameraView * instanceTransform * vec4(a_position, 1.0);

   vColor = a_color;
}
