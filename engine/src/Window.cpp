#include <midas/Window.hpp>

#include "internal/Sdl.hpp"
#include "internal/WindowNative.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace midas {

struct Window::Impl {
    std::string title;
    int width{0};
    int height{0};
    int pixel_width{0};
    int pixel_height{0};
    float pixel_density{1.0f};
    Native native;

    ~Impl() {
        if (native.window != nullptr) {
            SDL_DestroyWindow(native.window);
            native.window = nullptr;
        }
    }

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
};

Window::Window(std::string title, int width, int height)
    : impl_(std::make_unique<Impl>()) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Midas window size must be positive");
    }

    impl_->title = std::move(title);
    impl_->width = width;
    impl_->height = height;
    impl_->pixel_width = width;
    impl_->pixel_height = height;
    impl_->pixel_density = 1.0f;

    // HIGH_PIXEL_DENSITY: on macOS/Wayland this is a 2× (or more) framebuffer
    // while `width`/`height` stay in window coordinates. Logical presentation
    // still uses EngineConfig; mouse mapping must go through the renderer.
    impl_->native.window = SDL_CreateWindow(impl_->title.c_str(), width, height,
                                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (impl_->native.window == nullptr) {
        detail::throw_sdl("SDL_CreateWindow failed");
    }

    // Keep a usable client area so letterboxing never sees a 0×0 drawable.
    (void)SDL_SetWindowMinimumSize(impl_->native.window, 320, 180);
}

Window::~Window() = default;

std::string_view Window::title() const noexcept {
    return impl_->title;
}

int Window::width() const noexcept {
    return impl_->width;
}

int Window::height() const noexcept {
    return impl_->height;
}

int Window::pixel_width() const noexcept {
    return impl_->pixel_width;
}

int Window::pixel_height() const noexcept {
    return impl_->pixel_height;
}

float Window::pixel_density() const noexcept {
    return impl_->pixel_density;
}

Window::Native* Window::native() noexcept {
    return impl_ ? &impl_->native : nullptr;
}

const Window::Native* Window::native() const noexcept {
    return impl_ ? &impl_->native : nullptr;
}

void Window::sync_size_from_native() noexcept {
    if (impl_ == nullptr || impl_->native.window == nullptr) {
        return;
    }
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSize(impl_->native.window, &width, &height)) {
        return;
    }
    if (width > 0 && height > 0) {
        impl_->width = width;
        impl_->height = height;
    }

    int pixel_width = 0;
    int pixel_height = 0;
    if (SDL_GetWindowSizeInPixels(impl_->native.window, &pixel_width, &pixel_height) &&
        pixel_width > 0 && pixel_height > 0) {
        impl_->pixel_width = pixel_width;
        impl_->pixel_height = pixel_height;
    } else if (impl_->width > 0 && impl_->height > 0) {
        impl_->pixel_width = impl_->width;
        impl_->pixel_height = impl_->height;
    }

    const float density = SDL_GetWindowPixelDensity(impl_->native.window);
    impl_->pixel_density = (std::isfinite(density) && density > 0.0f) ? density : 1.0f;
}

}  // namespace midas
