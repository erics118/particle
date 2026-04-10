#include <metal_stdlib>

using namespace metal;

// must match PackedParticle in metal_renderer.cppm
// note: float2 has alignment 8, making sizeof = 16; use separate floats to match C++ stride of 12
struct VertexIn {
    float x;       // world-space x position (pixels)
    float y;       // world-space y position (pixels)
    float radius;  // world-space radius (pixels)
};

// must match ViewportUniforms in metal_renderer.cppm
struct ViewportUniforms {
    float width;
    float height;
};

struct VertexOutput {
    float4 position [[position]];
    float point_size [[point_size]];
};

vertex VertexOutput particle_vertex(
    const device VertexIn* vertices [[buffer(0)]],
    constant ViewportUniforms& viewport [[buffer(1)]],
    uint vertex_id [[vertex_id]]) {
    const VertexIn v = vertices[vertex_id];

    // convert world-space pixel coords to NDC
    const float ndc_x = (v.x / viewport.width) * 2.0 - 1.0;
    const float ndc_y = 1.0 - (v.y / viewport.height) * 2.0;

    VertexOutput output;
    output.position = float4(ndc_x, ndc_y, 0.0, 1.0);
    output.point_size = v.radius * 2.0;  // diameter in drawable pixels

    return output;
}

fragment float4 particle_fragment(float2 point_coord [[point_coord]]) {
    const float dist = length(point_coord - 0.5);

    // smooth edge over last 2px of the radius
    const float alpha = 1.0f - smoothstep(0.45, 0.5, dist);

    // white with soft edge
    return {1.0, 1.0, 1.0, alpha};
}
