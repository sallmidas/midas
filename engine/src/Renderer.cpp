#include <midas/Renderer.hpp>
#include <midas/Window.hpp>

#include "internal/RendererNative.hpp"
#include "internal/Sdl.hpp"
#include "internal/WindowNative.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace midas {

struct Renderer::Impl {
    Native native;
    Camera camera{};
    int logical_width{0};
    int logical_height{0};
};

Renderer::Renderer(Window& window) : impl_(std::make_unique<Impl>()) {
    auto* native = window.native();
    if (native == nullptr || native->window == nullptr) {
        throw std::runtime_error("Midas renderer requires a native window");
    }

    impl_->logical_width = window.width();
    impl_->logical_height = window.height();
    impl_->camera.position = {static_cast<float>(impl_->logical_width) * 0.5f,
                              static_cast<float>(impl_->logical_height) * 0.5f};
    impl_->camera.zoom = 1.0f;

    impl_->native.renderer = SDL_CreateRenderer(native->window, nullptr);
    if (impl_->native.renderer == nullptr) {
        detail::throw_sdl("SDL_CreateRenderer failed");
    }

    if (!SDL_SetRenderLogicalPresentation(impl_->native.renderer, window.width(), window.height(),
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        detail::throw_sdl("SDL_SetRenderLogicalPresentation failed");
    }

    (void)SDL_SetRenderVSync(impl_->native.renderer, 1);
}

Renderer::~Renderer() {
    if (impl_ && impl_->native.renderer != nullptr) {
        SDL_DestroyRenderer(impl_->native.renderer);
        impl_->native.renderer = nullptr;
    }
}

Renderer::Native* Renderer::native() noexcept {
    return impl_ ? &impl_->native : nullptr;
}

const Renderer::Native* Renderer::native() const noexcept {
    return impl_ ? &impl_->native : nullptr;
}

Rect Renderer::project_world(const Rect& world) const noexcept {
    return impl_->camera.project(world, static_cast<float>(impl_->logical_width),
                                 static_cast<float>(impl_->logical_height));
}

void Renderer::clear(const Color& color) {
    if (!SDL_SetRenderDrawColorFloat(impl_->native.renderer, color.r, color.g, color.b, color.a)) {
        detail::throw_sdl("SDL_SetRenderDrawColorFloat failed");
    }
    if (!SDL_RenderClear(impl_->native.renderer)) {
        detail::throw_sdl("SDL_RenderClear failed");
    }
}

void Renderer::fill_rect(const Rect& rect, const Color& color) {
    if (!SDL_SetRenderDrawColorFloat(impl_->native.renderer, color.r, color.g, color.b, color.a)) {
        detail::throw_sdl("SDL_SetRenderDrawColorFloat failed");
    }
    const Rect screen = project_world(rect);
    const SDL_FRect native_rect{screen.x, screen.y, screen.w, screen.h};
    if (!SDL_RenderFillRect(impl_->native.renderer, &native_rect)) {
        detail::throw_sdl("SDL_RenderFillRect failed");
    }
}

void Renderer::draw_texture(const Texture& texture, const Rect& dest, const Color& tint) {
    auto* native_texture = static_cast<SDL_Texture*>(texture.native_texture());
    if (native_texture == nullptr) {
        throw std::runtime_error("Midas draw_texture requires a valid texture");
    }

    if (!SDL_SetTextureColorModFloat(native_texture, tint.r, tint.g, tint.b)) {
        detail::throw_sdl("SDL_SetTextureColorModFloat failed");
    }
    if (!SDL_SetTextureAlphaModFloat(native_texture, tint.a)) {
        detail::throw_sdl("SDL_SetTextureAlphaModFloat failed");
    }

    const Rect screen = project_world(dest);
    const SDL_FRect native_rect{screen.x, screen.y, screen.w, screen.h};
    if (!SDL_RenderTexture(impl_->native.renderer, native_texture, nullptr, &native_rect)) {
        detail::throw_sdl("SDL_RenderTexture failed");
    }
}

void Renderer::present() {
    if (!SDL_RenderPresent(impl_->native.renderer)) {
        detail::throw_sdl("SDL_RenderPresent failed");
    }
}

Texture Renderer::create_texture(int width, int height, std::span<const std::uint8_t> rgba) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Midas texture size must be positive");
    }

    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if (rgba.size() < expected) {
        throw std::runtime_error("Midas texture pixel buffer is too small");
    }

    SDL_Texture* native = SDL_CreateTexture(impl_->native.renderer, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STATIC, width, height);
    if (native == nullptr) {
        detail::throw_sdl("SDL_CreateTexture failed");
    }

    const int pitch = width * 4;
    if (!SDL_UpdateTexture(native, nullptr, rgba.data(), pitch)) {
        SDL_DestroyTexture(native);
        detail::throw_sdl("SDL_UpdateTexture failed");
    }

    return Texture{native, width, height};
}

Texture Renderer::load_bmp(std::string_view path) {
    if (path.empty()) {
        throw std::runtime_error("Midas load_bmp requires a path");
    }

    const std::string path_str{path};
    SDL_Surface* loaded = SDL_LoadBMP(path_str.c_str());
    if (loaded == nullptr) {
        detail::throw_sdl("SDL_LoadBMP failed");
    }

    SDL_Surface* converted = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (converted == nullptr) {
        detail::throw_sdl("SDL_ConvertSurface failed");
    }

    SDL_Texture* native = SDL_CreateTextureFromSurface(impl_->native.renderer, converted);
    const int width = converted->w;
    const int height = converted->h;
    SDL_DestroySurface(converted);
    if (native == nullptr) {
        detail::throw_sdl("SDL_CreateTextureFromSurface failed");
    }

    return Texture{native, width, height};
}

void Renderer::set_camera(const Camera& camera) noexcept {
    impl_->camera = camera;
    impl_->camera.clamp_zoom();
}

const Camera& Renderer::camera() const noexcept {
    return impl_->camera;
}

}  // namespace midas
