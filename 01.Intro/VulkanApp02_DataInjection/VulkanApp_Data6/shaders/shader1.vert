#version 450

// From vertex input stage
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

// Uniform Buffer Object
layout(binding = 0) uniform ViewProjection {
    mat4 projection;
    mat4 view;
} viewProjection;

// Push constant
layout(push_constant) uniform PushModel {
    mat4 model;
} pushModel;

// To fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = viewProjection.projection * viewProjection.view *
		pushModel.model * vec4(pos, 1.0);
    fragColor = col;
}