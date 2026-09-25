#version 450

layout(set = 0, binding = 0) buffer ChunkData {
    vec4 lanes[];
} chunks;

layout(push_constant) uniform PC {
    uint entity_count;
    float time;
    mat4 vp;
} pc;

void main() {
    uint id = gl_VertexIndex;
    if (id >= pc.entity_count) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        return;
    }

    uint chunk = id >> 2;        // / 4 (lanes per vec4 in AoSoA)
    uint lane = id & 3u;         // % 4
    uint base = chunk * 16u;

    vec3 pos = vec3(
        chunks.lanes[base + 0u][lane],
        chunks.lanes[base + 1u][lane],
        chunks.lanes[base + 2u][lane]
    );

    vec4 clip = pc.vp * vec4(pos, 1.0);
    gl_Position = clip;

    // Depth-based point size (perspective scaling)
    float w = max(clip.w, 0.01);
    gl_PointSize = clamp(64.0 / w, 2.0, 32.0);
}
