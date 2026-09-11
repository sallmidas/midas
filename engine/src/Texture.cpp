#include <midas/Texture.hpp>

#include "internal/Sdl.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace midas {

struct Texture::Impl {
    SDL_Texture* texture{nullptr};
    int width{0};
    int height{0};
};

Texture::Texture(void* native_texture, int width, int height) : impl_(std::make_unique<Impl>()) {
    impl_->texture = static_cast<SDL_Texture*>(native_texture);
    impl_->width = width;
    impl_->height = height;

    if (impl_->texture == nullptr) {
        throw std::runtime_error("Midas texture requires a native handle");
    }

    (void)SDL_SetTextureBlendMode(impl_->texture, SDL_BLENDMODE_BLEND);
    (void)SDL_SetTextureScaleMode(impl_->texture, SDL_SCALEMODE_NEAREST);
}

Texture::Texture(Texture&& other) noexcept = default;

Texture& Texture::operator=(Texture&& other) noexcept = default;

Texture::~Texture() {
    if (impl_ && impl_->texture != nullptr) {
        SDL_DestroyTexture(impl_->texture);
        impl_->texture = nullptr;
    }
}

int Texture::width() const noexcept {
    return impl_ ? impl_->width : 0;
}

int Texture::height() const noexcept {
    return impl_ ? impl_->height : 0;
}

void* Texture::native_texture() const noexcept {
    return impl_ ? impl_->texture : nullptr;
}

std::vector<std::uint8_t> make_checkerboard_rgba(
    int width, int height, Color even, Color odd, int cell_size) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Midas checkerboard size must be positive");
    }
    if (cell_size <= 0) {
        throw std::runtime_error("Midas checkerboard cell size must be positive");
    }

    auto to_u8 = [](float channel) -> std::uint8_t {
        const float clamped = std::clamp(channel, 0.0f, 1.0f);
        return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
    };

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool use_even = ((x / cell_size) + (y / cell_size)) % 2 == 0;
            const Color& color = use_even ? even : odd;
            const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(x)) *
                                  4;
            pixels[i + 0] = to_u8(color.r);
            pixels[i + 1] = to_u8(color.g);
            pixels[i + 2] = to_u8(color.b);
            pixels[i + 3] = to_u8(color.a);
        }
    }
    return pixels;
}

}  // namespace midas
