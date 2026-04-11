#version 460

layout (location = 0) in vec3 a_coords;

layout(push_constant) uniform PushConstants {
    mat4 u_Transform;
} pc;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 u_CameraView;
    mat4 u_CameraProjection;
} camera;


void main(){
    gl_Position = camera.u_CameraProjection * camera.u_CameraView * pc.u_Transform * vec4(a_coords, 1.0);
}
