#include <metal_stdlib>

using namespace metal;

struct VertexOutput {
    float4 position [[position]];
    float point_size [[point_size]];
};

vertex VertexOutput particle_vertex(const device float2* positions [[buffer(0)]], uint vertex_id [[vertex_id]]) {
    VertexOutput output;

    // x,y = position provided, z = 0, w = 1
    output.position = float4(positions[vertex_id], 0.0, 1.0);

    // diameter
    output.point_size = 5.0;

    return output;
}

fragment float4 particle_fragment() {
    // white
    return {1.0, 1.0, 1.0, 1.0};
}
