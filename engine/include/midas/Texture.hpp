#pragma once

#include <midas/Types.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace midas {

class Renderer;

/// GPU texture uploaded from CPU pixels or a BMP file.
///
/// Sampling is nearest-neighbor (`SDL_SCALEMODE_NEAREST`) so pixel-art sprites
/// stay sharp when the camera zooms. Linear filtering is a later, opt-in layer.
///
/// **Lifetime:** destroy the texture before the `Renderer` that created it
/// (declare `Engine` first, then the `Texture`, so the texture dies first).
/// SDL frees leftover GPU textures with the renderer. A `Texture` destructor
/// after that skips `SDL_DestroyTexture` so reversed teardown cannot UAF.
/// Move-assignment is RAII-safe (the old GPU texture is released).
class Texture {
public:
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    ~Texture();

    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

private:
    friend class Renderer;

    Texture(void* native_texture, int width, int height, std::shared_ptr<void> gpu_lifetime);

    [[nodiscard]] void* native_texture() const noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Tightly packed RGBA8, row-major, 4 bytes per pixel. `cell_size` is the
/// checker square in pixels. Used when you want a GPU texture without a file.
[[nodiscard]] std::vector<std::uint8_t> make_checkerboard_rgba(
    int width, int height, Color even, Color odd, int cell_size);

}  // namespace midas
