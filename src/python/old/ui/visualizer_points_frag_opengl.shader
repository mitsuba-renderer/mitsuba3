#version 330

in vec4 frag_color;
out vec4 out_color;

void main() {
    if (frag_color.a == 0.0)
        discard;

    out_color = frag_color;
}
