#include <midas/Window.hpp>

#include "internal/Sdl.hpp"
#include "internal/WindowNative.hpp"

#include <stdexcept>
#include <utility>

namespace midas {

struct Window::Impl {
    std::string title;
    int width{0};
    int height{0};
    Native native;
};

Window::Window(std::string title, int width, int height)
    : impl_(std::make_unique<Impl>()) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Midas window size must be positive");
    }

    impl_->title = std::move(title);
    impl_->width = width;
    impl_->height = height;

    impl_->native.window = SDL_CreateWindow(impl_->title.c_str(), width, height, 0);
    if (impl_->native.window == nullptr) {
        detail::throw_sdl("SDL_CreateWindow failed");
    }
}

Window::~Window() {
    if (impl_ && impl_->native.window != nullptr) {
        SDL_DestroyWindow(impl_->native.window);
        impl_->native.window = nullptr;
    }
}

std::string_view Window::title() const noexcept {
    return impl_->title;
}

int Window::width() const noexcept {
    return impl_->width;
}

int Window::height() const noexcept {
    return impl_->height;
}

Window::Native* Window::native() noexcept {
    return impl_ ? &impl_->native : nullptr;
}

const Window::Native* Window::native() const noexcept {
    return impl_ ? &impl_->native : nullptr;
}

}  // namespace midas
