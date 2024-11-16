#version 450

layout(location = 0) in vec3 colorIn;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(colorIn / 255, 1.0);
}