#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 0) uniform sampler2D u_tex;
layout(push_constant) uniform PushConsts {
    vec2 u_tex_size;
    vec2 u_out_size;
    int u_sharpness;
    int u_effect;
} pc;

void main() {
    vec4 color;
    if (pc.u_sharpness == 1) {
        vec2 p = v_texcoord * pc.u_tex_size - 0.5;
        vec2 i = floor(p);
        vec2 f = p - i;
        vec2 f2 = f * f;
        vec2 f3 = f2 * f;
        vec2 w0 = f2 - 0.5 * (f3 + f);
        vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
        vec2 tc = (i + 0.5 + f) / pc.u_tex_size;
        color = texture(u_tex, tc);
    } else {
        color = texture(u_tex, v_texcoord);
    }
    if (pc.u_effect == 1) {
        if (mod(gl_FragCoord.y, 2.0) < 1.0) color.rgb *= 0.7;
    } else if (pc.u_effect == 2) {
        vec2 grid_val = mod(gl_FragCoord.xy, 2.0);
        if (grid_val.x < 1.0 || grid_val.y < 1.0) color.rgb *= 0.8;
    }
    out_color = color;
}
