#version 460

layout(location = 0) in vec3 v_normal_ws;
layout(location = 1) in vec3 v_position_ws;

layout(location = 0) out vec4 frag_color;

void main() {
    // Simple directional light from above-right
    vec3 light_dir = normalize(vec3(0.5, 1.0, -0.3));
    vec3 N = normalize(v_normal_ws);

    float diffuse = max(dot(N, light_dir), 0.0);
    float ambient = 0.15;

    // Color based on normal direction for visual variety
    vec3 base_color = vec3(0.7, 0.5, 0.3);
    vec3 color = base_color * (ambient + diffuse * 0.85);

    frag_color = vec4(color, 1.0);
}
