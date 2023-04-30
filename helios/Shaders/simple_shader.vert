#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 inTexCoord; 

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord; 

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec3 color;
} push;

void main() {
    gl_Position = ubo.view * push.modelMatrix * vec4(position, 1.0);
    fragColor = color;
    fragTexCoord = inTexCoord;
}