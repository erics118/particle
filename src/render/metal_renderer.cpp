module;

#include <Foundation/Foundation.hpp>
#include <Foundation/NSSharedPtr.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <QuartzCore/QuartzCore.hpp>
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

void MetalRenderer::draw(MTK::View* pView) {
    if (pView == nullptr || !_pCommandQueue) {
        return;
    }

    NS::SharedPtr<NS::AutoreleasePool> pPool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

    NS::SharedPtr<CA::MetalDrawable> pDrawable = NS::RetainPtr(pView->currentDrawable());
    NS::SharedPtr<MTL::RenderPassDescriptor> pRpd = NS::RetainPtr(pView->currentRenderPassDescriptor());

    if (!pRpd || !pDrawable) {
        return;
    }

    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd.get());

    // draw blue color that pulses over time
    phase_ += 0.01;
    const double pulse = 0.2 * std::sin(phase_);

    pRpd->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
        kBaseColor.red,
        kBaseColor.green,
        kBaseColor.blue + pulse,
        kBaseColor.alpha));

    pEnc->endEncoding();
    pCmd->presentDrawable(pDrawable.get());
    pCmd->commit();
}

}  // namespace render
