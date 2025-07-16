#version 400 core

layout (location = 0) in vec3 center;    // 3D position
layout (location = 1) in vec3 scale;     // Scale factors
layout (location = 2) in vec4 rotation;  // Quaternion rotation
layout (location = 3) in vec4 color;     // RGBA color

out VS_OUT {
    vec3 center;
    vec3 scale;
    vec4 rotation;
    vec4 color;
} vs_out;

void main() {
    vs_out.center = center;
    vs_out.scale = scale;
    vs_out.rotation = rotation;
    vs_out.color = color;
}