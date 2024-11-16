#version 450

layout(location = 0) in vec2 vertex;
layout(location = 1) in vec3 colorIn;
layout(location = 2) in vec2 pos;

layout(location = 0) out vec3 colorOut;

const int GAME_UNIT_BOUND = 1000;

vec2 change_coordinate_bounds(vec2 pos) {
    vec2 new_pos = (2 * pos / GAME_UNIT_BOUND - 1);
    return vec2(new_pos.x, -new_pos.y);
}

void main() {
    gl_Position = vec4(change_coordinate_bounds(vertex + pos), 0.0, 1.0);
    colorOut = colorIn;
}