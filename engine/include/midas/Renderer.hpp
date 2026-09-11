#pragma once

#include <midas/Camera.hpp>
#include <midas/Texture.hpp>
#include <midas/Types.hpp>

#include <memory>
#include <span>
#include <string_view>

namespace midas {

class Window;

/// 2D present path: clear, fill an axis-aligned rect, draw a textured quad, present.
///
/// `fill_rect` and `draw_texture` take **world** rectangles. The active
/// orthographic camera maps them into logical window pixels. Logical
/// presentation is letterboxed, so the drawable aspect stays
/// `logical_width : logical_height` even on a resized or Retina window.
class Renderer {
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    ~Renderer();

    void clear(const Color& color);
    void fill_rect(const Rect& rect, const Color& color);
    void draw_texture(const Texture& texture, const Rect& dest,
                      const Color& tint = Color::white());
    void present();

    /// Upload tightly packed RGBA8 pixels (`width * height * 4` bytes).
    /// Throws if the size is invalid or the buffer is too small — it does not
    /// silently pad or crop.
    [[nodiscard]] Texture create_texture(int width, int height, std::span<const std::uint8_t> rgba);

    /// Load an uncompressed BMP (SDL3 core; no SDL_image). Throws with the path
    /// in the message if the file is missing, unreadable, or not a BMP.
    [[nodiscard]] Texture load_bmp(std::string_view path);

    /// Copies `camera` and sanitizes zoom/position (clamps zoom, drops NaNs).
    void set_camera(const Camera& camera) noexcept;
    [[nodiscard]] const Camera& camera() const noexcept;

    /// Logical present size in pixels (the coordinate space of `Input` mouse
    /// positions and of `Camera` viewport arguments).
    [[nodiscard]] int logical_width() const noexcept;
    [[nodiscard]] int logical_height() const noexcept;

private:
    friend class Engine;

    explicit Renderer(Window& window);

    struct Native;
    Native* native() noexcept;
    const Native* native() const noexcept;

    [[nodiscard]] Rect project_world(const Rect& world) const noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
