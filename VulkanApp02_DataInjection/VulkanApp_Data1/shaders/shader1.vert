#version 450

// From vertex input stage
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

// To fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(pos, 1.0);
    fragColor = col;
}