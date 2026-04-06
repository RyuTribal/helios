#version 460

layout(location = 0) in vec2 TexCoords;
layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 1) uniform sampler2D u_ScreenTexture;

void main() {
    fragColor = texture(u_ScreenTexture, TexCoords);
}
