#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace midas {

class Renderer;

/// OS window. Native SDL handles stay in `Native` (engine-private).
///
/// Three sizes, three jobs (SDL high-DPI):
/// - `width()` / `height()` — live client size in **window coordinates**
///   (`SDL_GetWindowSize`). Changes on resize.
/// - `pixel_width()` / `pixel_height()` — drawable framebuffer
///   (`SDL_GetWindowSizeInPixels`). On a Retina / high-DPI panel this is
///   often 2× `width`/`height` (`pixel_density()`).
/// - `Renderer::logical_width/height` — letterboxed `EngineConfig` size.
///   Drawing, the camera, and `Input` mouse positions use **only** this.
///
/// Never pass window or pixel size to `Camera` / `zoom_toward`. Never multiply
/// mouse coordinates by `pixel_density()` — the engine already maps window
/// coords → logical present pixels (letterbox + DPI).
class Window {
public:
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
    ~Window();

    [[nodiscard]] std::string_view title() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] int pixel_width() const noexcept;
    [[nodiscard]] int pixel_height() const noexcept;
    /// Framebuffer pixels per window coordinate (`SDL_GetWindowPixelDensity`).
    /// 1 on a 1× panel; typically 2 on Retina. Not a mouse-scale factor.
    [[nodiscard]] float pixel_density() const noexcept;

private:
    friend class Engine;
    friend class Renderer;

    Window(std::string title, int width, int height);

    struct Native;
    Native* native() noexcept;
    const Native* native() const noexcept;

    void sync_size_from_native() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
