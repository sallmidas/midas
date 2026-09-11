#pragma once

#include <midas/Types.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace midas {

class Renderer;

/// GPU texture uploaded from CPU pixels or a BMP file.
///
/// Destroy the texture before the `Renderer` that created it. SDL destroys
/// remaining textures with the renderer; a later `Texture` destructor would
/// double-free.
class Texture {
public:
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    ~Texture();

    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

private:
    friend class Renderer;

    Texture(void* native_texture, int width, int height);

    [[nodiscard]] void* native_texture() const noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Tightly packed RGBA8, row-major, 4 bytes per pixel. `cell_size` is the
/// checker square in pixels.
[[nodiscard]] std::vector<std::uint8_t> make_checkerboard_rgba(
    int width, int height, Color even, Color odd, int cell_size);

}  // namespace midas
