module;

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

export module render.metal_renderer;

export namespace render {

struct FrameContext {
    CA::MetalDrawable* drawable;
    MTL::RenderPassDescriptor* rpd;
};

class MetalRenderer {
   private:
    NS::SharedPtr<MTL::CommandQueue> _pCommandQueue;
    NS::SharedPtr<MTL::Device> _pDevice;

    double phase_;
    int width_;
    int height_;

   public:
    MetalRenderer(MTL::Device* pDevice);

    void resize(int width, int height);
    void draw(const FrameContext& frame_context);
};

}  // namespace render
