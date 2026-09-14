#include <midas/Texture.hpp>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

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
