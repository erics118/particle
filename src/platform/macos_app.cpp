module;

#include <chrono>
#include <memory>

// define macros to ensure that the selector and class symbols are linked
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION

#include <AppKit/AppKit.hpp>
#include <Foundation/Foundation.hpp>
#include <MetalKit/MetalKit.hpp>

module platform.macos_app;

import render.metal_renderer;
import sim.particle_simulation;

namespace platform {

namespace {

constexpr float kFixedTimeStep = 1.0f / 120.0f;
constexpr float kMaxFrameDelta = 0.1f;  // caps delta to prevent spiral of death on slow frames
constexpr int kMaxSimulationStepsPerFrame = 8;

class ViewDelegate : public MTK::ViewDelegate {
   private:
    using Clock = std::chrono::steady_clock;

    std::unique_ptr<render::MetalRenderer> renderer_;
    sim::ParticleSimulation simulation_;
    Clock::time_point last_frame_time_;
    float accumulated_time_{};

   public:
    ViewDelegate(MTL::Device* device, MTK::View* view)
        : renderer_(std::make_unique<render::MetalRenderer>(device)),
          simulation_(sim::SimulationConfig{}),
          last_frame_time_(Clock::now()) {
        const CGSize drawable_size = view->drawableSize();

        renderer_->resize(static_cast<int>(drawable_size.width), static_cast<int>(drawable_size.height));

        simulation_.resize_bounds(static_cast<float>(drawable_size.width), static_cast<float>(drawable_size.height));
    }

    // render the frame
    void drawInMTKView(MTK::View* view) override {
        if (view == nullptr) {
            return;
        }

        NS::SharedPtr<CA::MetalDrawable> drawable = NS::RetainPtr(view->currentDrawable());
        NS::SharedPtr<MTL::RenderPassDescriptor> render_pass_descriptor = NS::RetainPtr(view->currentRenderPassDescriptor());

        if (!drawable || !render_pass_descriptor) {
            return;
        }

        auto frame_context = render::FrameContext{
            .drawable = drawable.get(),
            .render_pass_descriptor = render_pass_descriptor.get(),
        };

        tick_simulation();

        renderer_->draw(frame_context, simulation_.particles());
    }

    // handle window resizing
    void drawableSizeWillChange(MTK::View* /* view */, CGSize size) override {
        renderer_->resize(static_cast<int>(size.width), static_cast<int>(size.height));
        simulation_.resize_bounds(static_cast<float>(size.width),
                                  static_cast<float>(size.height));
    }

   private:
    void tick_simulation() {
        const Clock::time_point now = Clock::now();
        const std::chrono::duration<float> frame_delta = now - last_frame_time_;
        last_frame_time_ = now;

        accumulated_time_ += std::min(frame_delta.count(), kMaxFrameDelta);

        int step_count = 0;

        while (accumulated_time_ >= kFixedTimeStep && step_count < kMaxSimulationStepsPerFrame) {
            simulation_.step(kFixedTimeStep);
            accumulated_time_ -= kFixedTimeStep;
            ++step_count;
        }

        // drop accumulated time if we hit the step cap to avoid catching up indefinitely
        if (step_count == kMaxSimulationStepsPerFrame) {
            accumulated_time_ = 0.0f;
        }
    }
};

class AppDelegate final : public NS::ApplicationDelegate {
   private:
    NS::SharedPtr<NS::Window> window_;
    NS::SharedPtr<MTK::View> mtk_view_;
    NS::SharedPtr<MTL::Device> device_;

    std::unique_ptr<ViewDelegate> view_delegate_;

   public:
    void applicationDidFinishLaunching(NS::Notification* /* notification */) override {
        // create the system device
        device_ = NS::TransferPtr(MTL::CreateSystemDefaultDevice());

        // if fail to do so, terminate app
        if (!device_) {
            NS::Application::sharedApplication()->terminate(nullptr);
            return;
        }

        // starting  position and size
        const CGRect frame = {{0.0, 0.0}, {1280.0, 720.0}};

        // style mask for the window
        const auto style_mask =
            NS::WindowStyleMaskTitled |
            NS::WindowStyleMaskClosable |
            NS::WindowStyleMaskMiniaturizable |
            NS::WindowStyleMaskResizable;

        // create the actual window
        window_ = NS::TransferPtr(NS::Window::alloc()->init(
            frame,
            style_mask,
            NS::BackingStoreBuffered,
            false));

        // set window title
        window_->setTitle(MTLSTR("Particle"));

        // configure the metal view
        mtk_view_ = NS::TransferPtr(MTK::View::alloc()->init(frame, device_.get()));
        mtk_view_->setColorPixelFormat(MTL::PixelFormatBGRA8Unorm);
        mtk_view_->setPreferredFramesPerSecond(60);
        mtk_view_->setEnableSetNeedsDisplay(false);
        mtk_view_->setPaused(false);
        window_->setContentView(mtk_view_.get());

        // configure view delegate
        view_delegate_ = std::make_unique<ViewDelegate>(device_.get(), mtk_view_.get());
        mtk_view_->setDelegate(view_delegate_.get());

        window_->makeKeyAndOrderFront(nullptr);

        NS::Application::sharedApplication()->activateIgnoringOtherApps(true);
    }

    // terminate the app when the last window is closed
    bool applicationShouldTerminateAfterLastWindowClosed(NS::Application* /* sender */) override {
        return true;
    }
};

}  // namespace

int run_app() {
    NS::SharedPtr<NS::AutoreleasePool> autorelease_pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

    AppDelegate delegate;

    NS::Application* shared_application = NS::Application::sharedApplication();
    shared_application->setDelegate(&delegate);
    shared_application->run();

    return 0;
}

}  // namespace platform
