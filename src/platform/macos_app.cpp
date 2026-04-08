module;

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

namespace platform {

namespace {

class ViewDelegate : public MTK::ViewDelegate {
   private:
    std::unique_ptr<render::MetalRenderer> _pRenderer;

   public:
    ViewDelegate(MTL::Device* device, MTK::View* view) {
        _pRenderer = std::make_unique<render::MetalRenderer>(device);

        const CGSize drawable_size = view->drawableSize();
        _pRenderer->resize(static_cast<int>(drawable_size.width),
                           static_cast<int>(drawable_size.height));
    }

    // actually render the frame
    void drawInMTKView(MTK::View* pView) override {
        if (pView == nullptr) {
            return;
        }

        NS::SharedPtr<CA::MetalDrawable> pDrawable = NS::RetainPtr(pView->currentDrawable());
        NS::SharedPtr<MTL::RenderPassDescriptor> pRpd = NS::RetainPtr(pView->currentRenderPassDescriptor());

        if (!pDrawable || !pRpd) {
            return;
        }

        auto frame_context = render::FrameContext{
            .drawable = pDrawable.get(),
            .rpd = pRpd.get(),
        };

        _pRenderer->draw(frame_context);
    }

    // handle window resizing
    void drawableSizeWillChange(MTK::View* /* pView */, CGSize size) override {
        _pRenderer->resize(static_cast<int>(size.width), static_cast<int>(size.height));
    }
};

class AppDelegate final : public NS::ApplicationDelegate {
   private:
    NS::SharedPtr<NS::Window> _pWindow;
    NS::SharedPtr<MTK::View> _pMtkView;
    NS::SharedPtr<MTL::Device> _pDevice;

    std::unique_ptr<ViewDelegate> _pViewDelegate;

   public:
    void applicationDidFinishLaunching(NS::Notification* /* pNotification */) override {
        // create the system device
        _pDevice = NS::TransferPtr(MTL::CreateSystemDefaultDevice());

        // if fail to do so, terminate app
        if (!_pDevice) {
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
        _pWindow = NS::TransferPtr(NS::Window::alloc()->init(
            frame,
            style_mask,
            NS::BackingStoreBuffered,
            false));

        // set window title
        _pWindow->setTitle(MTLSTR("Particle"));

        // configure the metal view
        _pMtkView = NS::TransferPtr(MTK::View::alloc()->init(frame, _pDevice.get()));
        _pMtkView->setColorPixelFormat(MTL::PixelFormatBGRA8Unorm);
        _pMtkView->setPreferredFramesPerSecond(60);
        _pMtkView->setEnableSetNeedsDisplay(false);
        _pMtkView->setPaused(false);
        _pWindow->setContentView(_pMtkView.get());

        // configure view delegate
        _pViewDelegate = std::make_unique<ViewDelegate>(_pDevice.get(), _pMtkView.get());
        _pMtkView->setDelegate(_pViewDelegate.get());

        _pWindow->makeKeyAndOrderFront(nullptr);

        NS::Application::sharedApplication()->activateIgnoringOtherApps(true);
    }

    // terminate the app when the last window is closed
    bool applicationShouldTerminateAfterLastWindowClosed(NS::Application* /* pSender */) override {
        return true;
    }
};

}  // namespace

int run_app() {
    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    AppDelegate del;

    NS::Application* pSharedApplication = NS::Application::sharedApplication();
    pSharedApplication->setDelegate(&del);
    pSharedApplication->run();

    pAutoreleasePool->release();

    return 0;
}

}  // namespace platform
