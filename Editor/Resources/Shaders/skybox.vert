#version 460

layout (location = 0) in vec3 a_position;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 u_CameraView;
    mat4 u_CameraProjection;
    vec3 u_CameraPos;
    float u_CameraFarPlane;
    int u_NumDirectionalLights;
    int numberOfTilesX;
    float u_EnvironmentBrightness;
} global;

layout(location = 0) out vec3 local_pos;

void main()
{
    local_pos = a_position;

    mat4 rotView = mat4(mat3(global.u_CameraView));
    vec4 clipPos = global.u_CameraProjection * rotView * vec4(local_pos, 1.0);

    gl_Position = clipPos.xyww;
}
