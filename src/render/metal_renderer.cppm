module;

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

export module render.metal_renderer;

export namespace render {

struct FrameContext {
    CA::MetalDrawable* drawable;
    MTL::RenderPassDescriptor* render_pass_descriptor;
};

class MetalRenderer {
   private:
    NS::SharedPtr<MTL::CommandQueue> command_queue_;
    NS::SharedPtr<MTL::Device> device_;

    double phase_;
    int width_;
    int height_;

   public:
    MetalRenderer(MTL::Device* device);

    void resize(int width, int height);
    void draw(const FrameContext& frame_context);
};

}  // namespace render
