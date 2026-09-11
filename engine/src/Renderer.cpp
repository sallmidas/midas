#include <midas/Renderer.hpp>
#include <midas/Window.hpp>

#include "internal/Sdl.hpp"
#include "internal/WindowNative.hpp"

namespace midas {

struct Renderer::Impl {
    SDL_Renderer* renderer{nullptr};
};

Renderer::Renderer(Window& window) : impl_(std::make_unique<Impl>()) {
    auto* native = window.native();
    if (native == nullptr || native->window == nullptr) {
        throw std::runtime_error("Midas renderer requires a native window");
    }

    impl_->renderer = SDL_CreateRenderer(native->window, nullptr);
    if (impl_->renderer == nullptr) {
        detail::throw_sdl("SDL_CreateRenderer failed");
    }

    if (!SDL_SetRenderLogicalPresentation(impl_->renderer, window.width(), window.height(),
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        detail::throw_sdl("SDL_SetRenderLogicalPresentation failed");
    }

    (void)SDL_SetRenderVSync(impl_->renderer, 1);
}

Renderer::~Renderer() {
    if (impl_ && impl_->renderer != nullptr) {
        SDL_DestroyRenderer(impl_->renderer);
        impl_->renderer = nullptr;
    }
}

void Renderer::clear(const Color& color) {
    if (!SDL_SetRenderDrawColorFloat(impl_->renderer, color.r, color.g, color.b, color.a)) {
        detail::throw_sdl("SDL_SetRenderDrawColorFloat failed");
    }
    if (!SDL_RenderClear(impl_->renderer)) {
        detail::throw_sdl("SDL_RenderClear failed");
    }
}

void Renderer::fill_rect(const Rect& rect, const Color& color) {
    if (!SDL_SetRenderDrawColorFloat(impl_->renderer, color.r, color.g, color.b, color.a)) {
        detail::throw_sdl("SDL_SetRenderDrawColorFloat failed");
    }
    const SDL_FRect native_rect{rect.x, rect.y, rect.w, rect.h};
    if (!SDL_RenderFillRect(impl_->renderer, &native_rect)) {
        detail::throw_sdl("SDL_RenderFillRect failed");
    }
}

void Renderer::present() {
    if (!SDL_RenderPresent(impl_->renderer)) {
        detail::throw_sdl("SDL_RenderPresent failed");
    }
}

}  // namespace midas
