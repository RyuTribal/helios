#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_coords;

layout(location = 0) out vec2 TexCoords;

void main() {
    gl_Position = vec4(a_position, 1.0);
    TexCoords = a_texture_coords;
}
