#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec3 color;
} push;

void main() {
     //outColor = vec4(fragTexCoord, 0.0, 1.0);
    outColor = texture(texSampler, fragTexCoord);
}