#version 450

// From vertex input stage
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

// Uniform Buffer Object
layout(binding = 0) uniform ViewProjection {
    mat4 projection;
    mat4 view;
} viewProjection;

// Dynamic uniform buffer
layout(binding = 1) uniform Model {
    mat4 model;
} model;

// To fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = viewProjection.projection * viewProjection.view *
		model.model * vec4(pos, 1.0);
    fragColor = col;
}