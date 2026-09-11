#include <midas/Renderer.hpp>
#include <midas/Window.hpp>

#include "internal/GpuLifetime.hpp"
#include "internal/RendererNative.hpp"
#include "internal/Sdl.hpp"
#include "internal/WindowNative.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace midas {
namespace {

constexpr int kMaxTextureDimension = 16384;

Color clamp_color(Color color) noexcept {
    auto channel = [](float value, float fallback) noexcept {
        if (!std::isfinite(value)) {
            return fallback;
        }
        return std::clamp(value, 0.0f, 1.0f);
    };
    color.r = channel(color.r, 0.0f);
    color.g = channel(color.g, 0.0f);
    color.b = channel(color.b, 0.0f);
    color.a = channel(color.a, 1.0f);
    return color;
}

bool is_drawable_rect(const Rect& rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.w) &&
           std::isfinite(rect.h) && rect.w > 0.0f && rect.h > 0.0f;
}

void set_draw_color(SDL_Renderer* renderer, const Color& color) {
    const Color clamped = clamp_color(color);
    if (!SDL_SetRenderDrawColorFloat(renderer, clamped.r, clamped.g, clamped.b, clamped.a)) {
        detail::throw_sdl("SDL_SetRenderDrawColorFloat failed");
    }
}

}  // namespace

struct Renderer::Impl {
    Native native;
    Camera camera{};
    int logical_width{0};
    int logical_height{0};
    std::shared_ptr<detail::GpuLifetime> gpu{std::make_shared<detail::GpuLifetime>()};

    // RAII: constructor failures after SDL_CreateRenderer take this path too
    // (C++ does not run ~Renderer() when the constructor throws).
    ~Impl() {
        gpu->alive = false;
        if (native.renderer != nullptr) {
            SDL_DestroyRenderer(native.renderer);
            native.renderer = nullptr;
        }
    }

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
};

Renderer::Renderer(Window& window) : impl_(std::make_unique<Impl>()) {
    auto* native = window.native();
    if (native == nullptr || native->window == nullptr) {
        throw std::runtime_error("Midas renderer requires a native window");
    }

    // Captured once from the created window (EngineConfig). OS resizes change
    // `Window::width/height` and letterbox; these stay put so Camera/Input
    // keep a stable logical viewport.
    impl_->logical_width = window.width();
    impl_->logical_height = window.height();
    impl_->camera.position = {static_cast<float>(impl_->logical_width) * 0.5f,
                              static_cast<float>(impl_->logical_height) * 0.5f};
    impl_->camera.zoom = 1.0f;
    impl_->camera.sanitize();

    impl_->native.renderer = SDL_CreateRenderer(native->window, nullptr);
    if (impl_->native.renderer == nullptr) {
        detail::throw_sdl("SDL_CreateRenderer failed");
    }

    if (!SDL_SetRenderLogicalPresentation(impl_->native.renderer, window.width(), window.height(),
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        detail::throw_sdl("SDL_SetRenderLogicalPresentation failed");
    }

    // Alpha fills (HUD banner) and tinted sprites share this blend mode.
    if (!SDL_SetRenderDrawBlendMode(impl_->native.renderer, SDL_BLENDMODE_BLEND)) {
        detail::throw_sdl("SDL_SetRenderDrawBlendMode failed");
    }

    (void)SDL_SetRenderVSync(impl_->native.renderer, 1);
}

Renderer::~Renderer() = default;

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
    set_draw_color(impl_->native.renderer, color);
    if (!SDL_RenderClear(impl_->native.renderer)) {
        detail::throw_sdl("SDL_RenderClear failed");
    }
}

void Renderer::fill_rect(const Rect& rect, const Color& color) {
    const Rect screen = project_world(rect);
    if (!is_drawable_rect(screen)) {
        return;
    }
    set_draw_color(impl_->native.renderer, color);
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

    const Rect screen = project_world(dest);
    if (!is_drawable_rect(screen)) {
        return;
    }

    // Color/alpha mod is a per-texel multiply, independent of the projected dest
    // size — a gold tint stays gold at any camera zoom.
    const Color clamped = clamp_color(tint);
    if (!SDL_SetTextureColorModFloat(native_texture, clamped.r, clamped.g, clamped.b)) {
        detail::throw_sdl("SDL_SetTextureColorModFloat failed");
    }
    if (!SDL_SetTextureAlphaModFloat(native_texture, clamped.a)) {
        detail::throw_sdl("SDL_SetTextureAlphaModFloat failed");
    }

    const SDL_FRect native_rect{screen.x, screen.y, screen.w, screen.h};
    const bool drawn =
        SDL_RenderTexture(impl_->native.renderer, native_texture, nullptr, &native_rect);

    // Restore identity so a later draw of this texture cannot inherit the tint
    // (including the throw path below).
    (void)SDL_SetTextureColorModFloat(native_texture, 1.0f, 1.0f, 1.0f);
    (void)SDL_SetTextureAlphaModFloat(native_texture, 1.0f);

    if (!drawn) {
        detail::throw_sdl("SDL_RenderTexture failed");
    }
}

void Renderer::fill_screen_rect(const Rect& rect, const Color& color) {
    if (!is_drawable_rect(rect)) {
        return;
    }
    set_draw_color(impl_->native.renderer, color);
    const SDL_FRect native_rect{rect.x, rect.y, rect.w, rect.h};
    if (!SDL_RenderFillRect(impl_->native.renderer, &native_rect)) {
        detail::throw_sdl("SDL_RenderFillRect failed");
    }
}

void Renderer::draw_debug_text(Vec2 position, std::string_view text, const Color& color) {
    if (text.empty() || !std::isfinite(position.x) || !std::isfinite(position.y)) {
        return;
    }
    set_draw_color(impl_->native.renderer, color);
    const std::string owned{text};
    (void)SDL_RenderDebugText(impl_->native.renderer, position.x, position.y, owned.c_str());
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
    if (width > kMaxTextureDimension || height > kMaxTextureDimension) {
        throw std::runtime_error("Midas texture size is too large");
    }

    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if (rgba.size() < expected) {
        throw std::runtime_error("Midas texture pixel buffer is too small (" +
                                 std::to_string(rgba.size()) + " bytes, need " +
                                 std::to_string(expected) + ")");
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

    return Texture{native, width, height, impl_->gpu};
}

Texture Renderer::load_bmp(std::string_view path) {
    if (path.empty()) {
        throw std::runtime_error("Midas load_bmp requires a path");
    }

    const std::string path_str{path};
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path_str, ec)) {
        throw std::runtime_error("Midas load_bmp: file not found: " + path_str);
    }

    detail::UniqueSurface loaded{SDL_LoadBMP(path_str.c_str())};
    if (!loaded) {
        detail::throw_sdl("SDL_LoadBMP failed (" + path_str + ")");
    }

    detail::UniqueSurface converted{SDL_ConvertSurface(loaded.get(), SDL_PIXELFORMAT_RGBA32)};
    if (!converted) {
        detail::throw_sdl("SDL_ConvertSurface failed (" + path_str + ")");
    }

    if (converted->w <= 0 || converted->h <= 0) {
        throw std::runtime_error("Midas load_bmp: invalid dimensions in " + path_str);
    }
    if (converted->w > kMaxTextureDimension || converted->h > kMaxTextureDimension) {
        throw std::runtime_error("Midas load_bmp: image is too large: " + path_str);
    }

    SDL_Texture* native = SDL_CreateTextureFromSurface(impl_->native.renderer, converted.get());
    const int width = converted->w;
    const int height = converted->h;
    if (native == nullptr) {
        detail::throw_sdl("SDL_CreateTextureFromSurface failed (" + path_str + ")");
    }

    return Texture{native, width, height, impl_->gpu};
}

void Renderer::set_camera(const Camera& camera) noexcept {
    impl_->camera = camera;
    impl_->camera.sanitize();
}

const Camera& Renderer::camera() const noexcept {
    return impl_->camera;
}

int Renderer::logical_width() const noexcept {
    return impl_->logical_width;
}

int Renderer::logical_height() const noexcept {
    return impl_->logical_height;
}

Vec2 Renderer::logical_size() const noexcept {
    return {static_cast<float>(impl_->logical_width),
            static_cast<float>(impl_->logical_height)};
}

void Renderer::reapply_logical_presentation() noexcept {
    if (impl_ == nullptr || impl_->native.renderer == nullptr) {
        return;
    }
    if (impl_->logical_width <= 0 || impl_->logical_height <= 0) {
        return;
    }
    // Letterbox is the EngineConfig size, not the OS window. Re-apply after a
    // resize so a driver that drops presentation still matches Camera/Input.
    (void)SDL_SetRenderLogicalPresentation(impl_->native.renderer, impl_->logical_width,
                                           impl_->logical_height,
                                           SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

}  // namespace midas
