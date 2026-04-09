#include <metal_stdlib>

using namespace metal;

// must match ParticleUniforms in metal_renderer.cpp
struct ParticleUniforms {
    float point_size;
};

struct VertexOutput {
    float4 position [[position]];
    float point_size [[point_size]];
};

vertex VertexOutput particle_vertex(
    const device float2* positions [[buffer(0)]],
    constant ParticleUniforms& uniforms [[buffer(1)]],
    uint vertex_id [[vertex_id]]) {
    VertexOutput output;

    // x,y = position provided, z = 0, w = 1
    output.position = float4(positions[vertex_id], 0.0, 1.0);

    // diameter
    output.point_size = uniforms.point_size;

    return output;
}

fragment float4 particle_fragment(float2 point_coord [[point_coord]]) {
    const float dist = length(point_coord - 0.5);

    // smooth edge over last 2px of the radius
    const float alpha = 1.0f - smoothstep(0.45, 0.5, dist);

    // white with soft edge
    return {1.0, 1.0, 1.0, alpha};
}
