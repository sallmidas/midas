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
/// `fill_rect` and `draw_texture` take **world** rectangles and share one
/// projection (`Camera::project`), so a solid and a tinted sprite of the same
/// world rect stay the same size and position at any zoom. Tint is a per-texel
/// multiply — it does not depend on dest size.
///
/// Drawing happens in a **logical** 2D space (`logical_width` × `logical_height`,
/// the `EngineConfig` window size). SDL letterboxes that onto the real drawable
/// (`SDL_LOGICAL_PRESENTATION_LETTERBOX`): black bars if the window pixel aspect
/// differs; extra pixels on a Retina panel whose aspect still matches. Camera
/// math and mouse coordinates use this logical size, not raw drawable pixels
/// and not the live `Window` size after a resize. HUD helpers
/// (`fill_screen_rect`, `draw_debug_text`) skip the camera.
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

    /// Axis-aligned fill in **logical present pixels** (ignores the camera).
    /// Use for HUD / debug chrome that must not pan or zoom with the world.
    void fill_screen_rect(const Rect& rect, const Color& color);

    /// 8×8 debug bitmap text in logical present pixels (ignores the camera).
    /// Wraps `SDL_RenderDebugText` — a teaching HUD, not a game font atlas.
    /// Failure is ignored so a dummy/headless driver cannot abort the demo.
    void draw_debug_text(Vec2 position, std::string_view text,
                         const Color& color = Color::white());

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
    /// positions and of `Camera` viewport arguments). This is the
    /// `EngineConfig` window size; it does **not** follow OS resizes.
    /// Letterboxing pads the extra drawable. Pass these (or `logical_size()`)
    /// to `Camera`, never `Window::width/height`.
    [[nodiscard]] int logical_width() const noexcept;
    [[nodiscard]] int logical_height() const noexcept;
    [[nodiscard]] Vec2 logical_size() const noexcept;

private:
    friend class Engine;

    explicit Renderer(Window& window);

    struct Native;
    Native* native() noexcept;
    const Native* native() const noexcept;

    [[nodiscard]] Rect project_world(const Rect& world) const noexcept;

    void reapply_logical_presentation() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
