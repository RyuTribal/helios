#version 460

layout(location = 0) in vec2 TextureCoordinates;

layout(set = 1, binding = 1) uniform sampler2D hdrBuffer;

layout(push_constant) uniform PushConstants {
    float exposure;
} pc;

layout(location = 0) out vec4 fragColor;

// Uses Filmic tonemapping (thanks chat gpt)

vec3 FilmicToneMapping(vec3 color) {
    color = max(vec3(0.0), color - 0.004);
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);
    return color;
}

void main() {
    vec3 color = texture(hdrBuffer, TextureCoordinates).rgb;
    vec3 result = vec3(1.0) - exp(-color * pc.exposure);

    // Minor gamma correction. Need to expand on it
    FilmicToneMapping(result);
    fragColor = vec4(result, 1.0);
}
