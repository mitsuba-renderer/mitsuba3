#version 330

uniform mat4 mvp;

in vec3 position;
in vec3 color;
out vec4 frag_color;

void main() {
    gl_Position = mvp * vec4(position, 1.0);

    if (isnan(position.r)) /* nan (missing value) */
        frag_color = vec4(0.0);
    else
        frag_color = vec4(color, 1.0);

    gl_PointSize = 3.0;
}
