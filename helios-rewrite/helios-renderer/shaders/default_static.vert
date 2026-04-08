#version 460

layout (location = 0) in vec3 a_coords;
layout (location = 1) in vec4 a_colors;
layout (location = 2) in vec2 a_texture_coords;
layout (location = 3) in vec3 a_normals;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;

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

layout(location = 0) out vec3 worldSpacePosition;
layout(location = 1) out vec4 vertex_color;
layout(location = 2) out vec3 normal;
layout(location = 3) out mat3 TBN; // takes locations 3, 4, 5
layout(location = 6) out vec2 texCoords;
layout(location = 7) out vec3 cameraPosition;


void main() {
    gl_Position = global.u_CameraProjection * global.u_CameraView * pc.u_Transform * vec4(a_coords, 1.0);
    worldSpacePosition = vec3(pc.u_Transform * vec4(a_coords, 1.0));

    vec3 N = normalize(mat3(pc.u_Transform) * a_normals);
    vec3 T = normalize(mat3(pc.u_Transform) * a_tangent);
    vec3 B = normalize(mat3(pc.u_Transform) * a_bitangent);
    TBN = mat3(T, B, N);

    normal = N;

    cameraPosition = global.u_CameraPos;
    texCoords = a_texture_coords;
    vertex_color = a_colors;
}
