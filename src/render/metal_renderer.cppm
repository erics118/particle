module;

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <vector>

export module render.metal_renderer;

import sim.particle_simulation;

export namespace render {

struct FrameContext {
    CA::MetalDrawable* drawable;
    MTL::RenderPassDescriptor* render_pass_descriptor;
};

// must match VertexIn in particle_points.metal
struct PackedParticle {
    float x;
    float y;
    float radius;
};

// must match ViewportUniforms in particle_points.metal
struct ViewportUniforms {
    float width;
    float height;
};

class MetalRenderer {
   private:
    NS::SharedPtr<MTL::CommandQueue> command_queue_;
    NS::SharedPtr<MTL::Device> device_;
    NS::SharedPtr<MTL::RenderPipelineState> pipeline_state_;
    NS::SharedPtr<MTL::Buffer> particle_buffer_;

    std::vector<PackedParticle> packed_particles_;

    double phase_{};
    int width_{};
    int height_{};

   public:
    MetalRenderer(MTL::Device* device);

    void resize(int width, int height);
    void draw(const FrameContext& frame_context, sim::ConstParticleView particles);
};

}  // namespace render
