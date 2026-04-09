module;

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <cmath>
#include <cstring>
#include <ranges>

module render.metal_renderer;

namespace render {

const MTL::ClearColor kBaseColor = {0.06, 0.08, 0.12, 1.0};

namespace {

// NOLINTNEXTLINE(*-avoid-c-arrays)
constexpr char kShaderSource[] = {

#embed "shaders/particle_points.metal"

    , 0};

// compile embedded shader source and configure the render pipeline
NS::SharedPtr<MTL::RenderPipelineState> create_pipeline_state(MTL::Device* device) {
    NS::Error* error = nullptr;
    NS::SharedPtr<NS::String> shader_source =
        NS::TransferPtr(NS::String::alloc()->init(static_cast<const char*>(kShaderSource), NS::UTF8StringEncoding));

    NS::SharedPtr<MTL::Library> library = NS::TransferPtr(device->newLibrary(shader_source.get(), nullptr, &error));

    if (!library) {
        return nullptr;
    }

    NS::SharedPtr<MTL::Function> vertex_function =
        NS::TransferPtr(library->newFunction(NS::String::string("particle_vertex", NS::UTF8StringEncoding)));
    NS::SharedPtr<MTL::Function> fragment_function =
        NS::TransferPtr(library->newFunction(NS::String::string("particle_fragment", NS::UTF8StringEncoding)));

    if (!vertex_function || !fragment_function) {
        return nullptr;
    }

    NS::SharedPtr<MTL::RenderPipelineDescriptor> pipeline_descriptor = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());

    pipeline_descriptor->setVertexFunction(vertex_function.get());
    pipeline_descriptor->setFragmentFunction(fragment_function.get());

    MTL::RenderPipelineColorAttachmentDescriptor* color = pipeline_descriptor->colorAttachments()->object(0);

    // set color format
    color->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    // enable alpha blending for antialiased circle edges
    // result = src.rgb * src.a + dst.rgb * (1 - src.a)
    color->setBlendingEnabled(true);
    color->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    color->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    color->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
    color->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

    return NS::TransferPtr(device->newRenderPipelineState(pipeline_descriptor.get(), &error));
}

// this struct has to match ParticleUniforms in [particle_points.metal]
struct ParticleUniforms {
    float point_size;
};

// transform a single particle position from pixel coords to NDC
PackedParticlePosition pixel_to_ndc(float px, float py, float width, float height) {
    return {
        .x = (px / width) * 2.0f - 1.0f,
        .y = 1.0f - (py / height) * 2.0f,
    };
}

}  // namespace

MetalRenderer::MetalRenderer(MTL::Device* device)
    : command_queue_(NS::TransferPtr(device->newCommandQueue())),
      device_(NS::RetainPtr(device)),
      pipeline_state_(create_pipeline_state(device)),
      particle_buffer_(nullptr) {}

void MetalRenderer::resize(int width, int height) {
    width_ = width;
    height_ = height;
}

void MetalRenderer::draw(const FrameContext& frame_context, sim::ConstParticleView particles) {
    if (frame_context.drawable == nullptr ||
        frame_context.render_pass_descriptor == nullptr ||
        !pipeline_state_ ||
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

    const auto width = static_cast<float>(width_);
    const auto height = static_cast<float>(height_);

    packed_positions_.resize(particles.x.size());

    for (const auto& [pos, px, py] : std::views::zip(packed_positions_, particles.x, particles.y)) {
        pos = pixel_to_ndc(px, py, width, height);
    }

    // no particles to draw
    if (packed_positions_.empty()) {
        encoder->endEncoding();
        command_buffer->presentDrawable(frame_context.drawable);
        command_buffer->commit();
        return;
    }

    const std::size_t particle_data_size = std::span{packed_positions_}.size_bytes();

    // only reallocate when the buffer is too small
    if (!particle_buffer_ || particle_buffer_->length() < particle_data_size) {
        particle_buffer_ = NS::TransferPtr(device_->newBuffer(
            particle_data_size,
            MTL::ResourceStorageModeShared));

        if (!particle_buffer_) {
            encoder->endEncoding();
            command_buffer->presentDrawable(frame_context.drawable);
            command_buffer->commit();
            return;
        }
    }

    std::memcpy(particle_buffer_->contents(), packed_positions_.data(), particle_data_size);

    const ParticleUniforms uniforms{.point_size = particle_size_};

    encoder->setRenderPipelineState(pipeline_state_.get());
    encoder->setVertexBuffer(particle_buffer_.get(), 0, 0);
    encoder->setVertexBytes(&uniforms, sizeof(uniforms), 1);
    encoder->drawPrimitives(MTL::PrimitiveTypePoint, NS::UInteger{0}, static_cast<NS::UInteger>(packed_positions_.size()));

    encoder->endEncoding();
    command_buffer->presentDrawable(frame_context.drawable);
    command_buffer->commit();
}

}  // namespace render
