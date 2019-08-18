#version 330
uniform mat4 mvp;
in vec2 position;
out vec2 uv;
void main() {
    gl_Position = mvp * vec4(position, 0.0, 1.0);
    uv = position;
}
