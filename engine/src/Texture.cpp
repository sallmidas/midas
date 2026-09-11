#include <midas/Texture.hpp>

#include "internal/GpuLifetime.hpp"
#include "internal/Sdl.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

namespace midas {
namespace {

constexpr int kMaxTextureDimension = 16384;

void check_texture_size(int width, int height, const char* what) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error(std::string(what) + " size must be positive");
    }
    if (width > kMaxTextureDimension || height > kMaxTextureDimension) {
        throw std::runtime_error(std::string(what) + " size is too large");
    }
}

}  // namespace

struct Texture::Impl {
    SDL_Texture* texture{nullptr};
    int width{0};
    int height{0};
    std::shared_ptr<detail::GpuLifetime> gpu;

    ~Impl() {
        if (texture == nullptr) {
            return;
        }
        // Renderer already gone → SDL_DestroyRenderer freed leftover textures.
        if (!gpu || gpu->alive) {
            SDL_DestroyTexture(texture);
        }
        texture = nullptr;
    }

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
};

Texture::Texture(void* native_texture, int width, int height, std::shared_ptr<void> gpu_lifetime)
    : impl_(std::make_unique<Impl>()) {
    impl_->texture = static_cast<SDL_Texture*>(native_texture);
    impl_->width = width;
    impl_->height = height;
    impl_->gpu = std::static_pointer_cast<detail::GpuLifetime>(std::move(gpu_lifetime));

    if (impl_->texture == nullptr) {
        throw std::runtime_error("Midas texture requires a native handle");
    }

    // Nearest-neighbor: one texel → one (or many) pixels. Linear would blur
    // pixel art the moment the camera zoom is not 1.
    if (!SDL_SetTextureBlendMode(impl_->texture, SDL_BLENDMODE_BLEND)) {
        detail::throw_sdl("SDL_SetTextureBlendMode failed");
    }
    if (!SDL_SetTextureScaleMode(impl_->texture, SDL_SCALEMODE_NEAREST)) {
        detail::throw_sdl("SDL_SetTextureScaleMode failed");
    }
}

Texture::Texture(Texture&& other) noexcept = default;

Texture& Texture::operator=(Texture&& other) noexcept = default;

Texture::~Texture() = default;

int Texture::width() const noexcept {
    return impl_ ? impl_->width : 0;
}

int Texture::height() const noexcept {
    return impl_ ? impl_->height : 0;
}

bool Texture::valid() const noexcept {
    return impl_ && impl_->texture != nullptr;
}

void* Texture::native_texture() const noexcept {
    return impl_ ? impl_->texture : nullptr;
}

std::vector<std::uint8_t> make_checkerboard_rgba(
    int width, int height, Color even, Color odd, int cell_size) {
    check_texture_size(width, height, "Midas checkerboard");
    if (cell_size <= 0) {
        throw std::runtime_error("Midas checkerboard cell size must be positive");
    }

    auto to_u8 = [](float channel, float non_finite_fallback) -> std::uint8_t {
        if (!std::isfinite(channel)) {
            channel = non_finite_fallback;
        }
        return static_cast<std::uint8_t>(clamp01(channel) * 255.0f + 0.5f);
    };

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool use_even = ((x / cell_size) + (y / cell_size)) % 2 == 0;
            const Color& color = use_even ? even : odd;
            const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(x)) *
                                  4;
            pixels[i + 0] = to_u8(color.r, 0.0f);
            pixels[i + 1] = to_u8(color.g, 0.0f);
            pixels[i + 2] = to_u8(color.b, 0.0f);
            pixels[i + 3] = to_u8(color.a, 1.0f);
        }
    }
    return pixels;
}

}  // namespace midas
