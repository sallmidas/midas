#pragma once

#include <midas/Types.hpp>

#include <cstdint>
#include <vector>

namespace midas {

/// Opaque handle to a GPU texture owned by `Renderer`.
///
/// Copy it freely — it does not own the image. `create_texture` / `load_bmp`
/// mint ids; `draw_texture` looks them up. Default `{0, 0}` is invalid
/// (`generation == 0`): an `Entity` with that id is a solid fill, and
/// `draw_texture` skips (no crash).
///
/// **Lifetime:** destroying the `Renderer` / `Engine` that issued an id
/// invalidates it. A stale index/generation pair also skips the draw. Ids
/// are not valid on a different renderer. Sampling is nearest-neighbor
/// (`SDL_SCALEMODE_NEAREST`); draw one cell of a sheet with
/// `Renderer::draw_texture(..., src)` — `src` is in texture pixels; the GPU
/// image is not re-uploaded per cell.
struct TextureId {
    std::uint32_t index{0};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] friend constexpr bool operator==(TextureId, TextureId) noexcept = default;
};

/// Tightly packed RGBA8, row-major, 4 bytes per pixel. `cell_size` is the
/// checker square in pixels. Used when you want a GPU texture without a file.
/// Non-finite RGB channels become 0; non-finite alpha becomes 1 (same fallback
/// as the renderer). Finite channels are `clamp01`'d.
[[nodiscard]] std::vector<std::uint8_t> make_checkerboard_rgba(
    int width, int height, Color even, Color odd, int cell_size);

}  // namespace midas
