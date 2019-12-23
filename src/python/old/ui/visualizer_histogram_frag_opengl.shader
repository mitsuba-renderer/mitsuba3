#version 330
out vec4 out_color;
uniform sampler2D tex;
in vec2 uv;
/* http://paulbourke.net/texture_colour/colourspace/ */
vec3 colormap(float v, float vmin, float vmax) {
    vec3 c = vec3(1.0);
    if (v < vmin)
        v = vmin;
    if (v > vmax)
        v = vmax;
    float dv = vmax - vmin;
    if (v < (vmin + 0.25 * dv)) {
        c.r = 0.0;
        c.g = 4.0 * (v - vmin) / dv;
    } else if (v < (vmin + 0.5 * dv)) {
        c.r = 0.0;
        c.b = 1.0 + 4.0 * (vmin + 0.25 * dv - v) / dv;
    } else if (v < (vmin + 0.75 * dv)) {
        c.r = 4.0 * (v - vmin - 0.5 * dv) / dv;
        c.b = 0.0;
    } else {
        c.g = 1.0 + 4.0 * (vmin + 0.75 * dv - v) / dv;
        c.b = 0.0;
    }
    return c;
}

void main() {
    float value = texture(tex, uv).r;
    out_color = vec4(colormap(value, 0.0, 1.0), 1.0);
}
