module;

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <cmath>

module render.metal_renderer;

namespace render {

const MTL::ClearColor kBaseColor = {0.06, 0.08, 0.12, 1.0};

MetalRenderer::MetalRenderer(MTL::Device* device)
    : command_queue_(NS::TransferPtr(device->newCommandQueue())),
      device_(NS::RetainPtr(device)),
      phase_(0.0),
      width_(0),
      height_(0) {}

void MetalRenderer::resize(int width, int height) {
    width_ = width;
    height_ = height;
}

void MetalRenderer::draw(const FrameContext& frame_context) {
    if (frame_context.drawable == nullptr ||
        frame_context.render_pass_descriptor == nullptr ||
        !command_queue_) {
        return;
    }

    NS::SharedPtr<NS::AutoreleasePool> autorelease_pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

    // draw blue color that pulses over time
    phase_ += 0.01;
    const double pulse = 0.2 * std::sin(phase_);

    frame_context.render_pass_descriptor->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
        kBaseColor.red,
        kBaseColor.green,
        kBaseColor.blue + pulse,
        kBaseColor.alpha));

    MTL::CommandBuffer* command_buffer = command_queue_->commandBuffer();
    MTL::RenderCommandEncoder* encoder =
        command_buffer->renderCommandEncoder(frame_context.render_pass_descriptor);

    if (encoder == nullptr) {
        return;
    }

    if (width_ > 0 && height_ > 0) {
        encoder->setViewport(MTL::Viewport{
            .originX = 0.0,
            .originY = 0.0,
            .width = static_cast<double>(width_),
            .height = static_cast<double>(height_),
            .znear = 0.0,
            .zfar = 1.0,
        });
    }

    encoder->endEncoding();
    command_buffer->presentDrawable(frame_context.drawable);
    command_buffer->commit();
}

}  // namespace render
