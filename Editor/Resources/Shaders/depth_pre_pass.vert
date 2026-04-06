#version 460

layout (location = 0) in vec3 a_coords;

layout(push_constant) uniform PushConstants {
    mat4 u_Transform;
} pc;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 u_CameraView;
    mat4 u_CameraProjection;
    vec3 u_CameraPos;
    float u_CameraFarPlane;
    int u_NumDirectionalLights;
    int numberOfTilesX;
    float u_EnvironmentBrightness;
} global;


void main(){
    gl_Position = global.u_CameraProjection * global.u_CameraView * pc.u_Transform * vec4(a_coords, 1.0);
}
