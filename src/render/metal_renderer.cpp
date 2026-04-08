module;

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <cmath>

module render.metal_renderer;

namespace render {

const MTL::ClearColor kBaseColor = {0.06, 0.08, 0.12, 1.0};

MetalRenderer::MetalRenderer(MTL::Device* pDevice)
    : _pCommandQueue(NS::TransferPtr(pDevice->newCommandQueue())),
      _pDevice(NS::RetainPtr(pDevice)),
      phase_(0.0),
      width_(0),
      height_(0) {}

void MetalRenderer::resize(int width, int height) {
    width_ = width;
    height_ = height;
}

void MetalRenderer::draw(const FrameContext& frame_context) {
    if (frame_context.drawable == nullptr ||
        frame_context.rpd == nullptr ||
        !_pCommandQueue) {
        return;
    }

    NS::SharedPtr<NS::AutoreleasePool> pPool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

    // draw blue color that pulses over time
    phase_ += 0.01;
    const double pulse = 0.2 * std::sin(phase_);

    frame_context.rpd->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
        kBaseColor.red,
        kBaseColor.green,
        kBaseColor.blue + pulse,
        kBaseColor.alpha));

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    MTL::RenderCommandEncoder* pEnc =
        pCmd->renderCommandEncoder(frame_context.rpd);

    if (pEnc == nullptr) {
        return;
    }

    if (width_ > 0 && height_ > 0) {
        pEnc->setViewport(MTL::Viewport{
            .originX = 0.0,
            .originY = 0.0,
            .width = static_cast<double>(width_),
            .height = static_cast<double>(height_),
            .znear = 0.0,
            .zfar = 1.0,
        });
    }

    pEnc->endEncoding();
    pCmd->presentDrawable(frame_context.drawable);
    pCmd->commit();
}

}  // namespace render
