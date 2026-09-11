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

    impl_->native.window = SDL_CreateWindow(impl_->title.c_str(), width, height,
                                            SDL_WINDOW_RESIZABLE);
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
}

}  // namespace midas
