#version 450

// Input colors from vertex shader
layout(location = 0) in vec3 fragColor;

// Final output color, must have location
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}