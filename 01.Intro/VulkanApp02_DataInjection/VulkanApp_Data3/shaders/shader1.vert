#version 450

// From vertex input stage
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

// Uniform Buffer Object
// We pass a full object, not just a value like in OpenGL
layout(binding = 0) uniform MVP {
    mat4 projection;
    mat4 view;
    mat4 model;
} mvp;

// To fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = mvp.projection * mvp.view * mvp.model * vec4(pos, 1.0);
    fragColor = col;
}