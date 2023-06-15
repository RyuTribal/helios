#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 instanceColor;
layout(location = 2) in vec2 instanceTexCoord; 

layout(location = 3) in vec4 instanceTransformX; 
layout(location = 4) in vec4 instanceTransformY; 
layout(location = 5) in vec4 instanceTransformZ; 
layout(location = 6) in vec4 instanceTransformTranslation; 


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
    mat4 instanceTransform = mat4(instanceTransformX, instanceTransformY, instanceTransformZ, instanceTransformTranslation);
    gl_Position = ubo.view * instanceTransform * vec4(position, 1.0);
    fragColor = instanceColor;
    fragTexCoord = instanceTexCoord;
}
