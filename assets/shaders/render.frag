#version 450

layout(location = 0) out vec4 out_color;

void main() {
    vec2 uv = gl_PointCoord - vec2(0.5);
    float dist_sq = dot(uv, uv);
    if (dist_sq > 0.25) discard;

    float alpha = 1.0 - smoothstep(0.0, 0.25, dist_sq);
    // Mavi-beyaz parıltı
    vec3 color = mix(vec3(0.35, 0.65, 1.0), vec3(1.0), alpha * 0.4);
    out_color = vec4(color, alpha);
}
