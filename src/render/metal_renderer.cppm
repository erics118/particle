module;

#include <Metal/MTLDevice.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

export module render.metal_renderer;

export namespace render {

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
    void draw(MTK::View* pView);
};

}  // namespace render
