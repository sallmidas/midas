#pragma once

#include <midas/Camera.hpp>
#include <midas/Texture.hpp>
#include <midas/Types.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace midas {

class Window;

/// 2D present path: clear, fill an axis-aligned rect, draw a textured quad, present.
///
/// `fill_rect` and `draw_texture` take **world** dest rectangles and share one
/// projection (`Camera::project`), so a solid and a tinted sprite of the same
/// world rect stay the same size and position at any zoom. Tint is a per-texel
/// multiply — it does not depend on dest size. An optional **source** `Rect` is
/// in texture pixel space (atlas cell); omit it to draw the whole texture.
///
/// GPU textures live in this renderer. `create_texture` / `load_bmp` return a
/// `TextureId`; `draw_texture` resolves that handle (invalid or stale → skip,
/// no crash). Destroying the renderer invalidates every id it issued.
///
/// Drawing happens in **logical present pixels** (`logical_width` ×
/// `logical_height`, captured from `EngineConfig`). That is not live
/// `Window::width` (window coordinates) and not `Window::pixel_width`
/// (framebuffer). SDL letterboxes the logical view onto the drawable
/// (`SDL_LOGICAL_PRESENTATION_LETTERBOX`): black bars if the aspect differs;
/// extra framebuffer pixels on a Retina panel (`Window::pixel_density` often
/// 2) whose window aspect still matches. Camera math and mouse coordinates
/// use this logical size. HUD helpers (`fill_screen_rect`, `draw_debug_text`)
/// skip the camera.
class Renderer {
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    ~Renderer();

    void clear(const Color& color);
    void fill_rect(const Rect& rect, const Color& color);
    /// Textured quad. `dest` is world space (camera-projected). The whole
    /// texture is sampled. Tint is a per-texel multiply (default white).
    /// Invalid / stale `texture` is skipped (no crash).
    void draw_texture(TextureId texture, const Rect& dest,
                      const Color& tint = Color::white());
    /// Same as the three-argument draw, but `src` is a rectangle in **texture
    /// pixel space** (top-left origin, same units as `texture_width` /
    /// `texture_height`). Use this for sprite-sheet cells: one GPU upload,
    /// many draws. An empty `src` (`{}`) samples the whole texture. Any other
    /// non-finite or non-positive `src` is skipped (same drawable rule as dest).
    void draw_texture(TextureId texture, const Rect& dest, const Rect& src,
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
    /// silently pad or crop. The renderer owns the GPU image; the returned
    /// handle is valid until this renderer is destroyed.
    [[nodiscard]] TextureId create_texture(int width, int height,
                                           std::span<const std::uint8_t> rgba);

    /// Load an uncompressed BMP (SDL3 core; no SDL_image). Throws with the path
    /// in the message if the file is missing, unreadable, or not a BMP.
    [[nodiscard]] TextureId load_bmp(std::string_view path);

    /// True if `id` was minted by this renderer and the slot is still live.
    [[nodiscard]] bool texture_valid(TextureId id) const noexcept;
    /// Pixel size of a live texture; 0 if `id` is invalid or stale.
    [[nodiscard]] int texture_width(TextureId id) const noexcept;
    [[nodiscard]] int texture_height(TextureId id) const noexcept;

    /// Copies `camera` and sanitizes zoom/position (clamps zoom, drops non-finite pan).
    void set_camera(const Camera& camera) noexcept;
    [[nodiscard]] const Camera& camera() const noexcept;

    /// Logical present size in pixels (the coordinate space of `Input` mouse
    /// positions and of `Camera` viewport arguments). This is the
    /// `EngineConfig` window size; it does **not** follow OS resizes.
    /// Letterboxing pads the extra drawable. Pass these (or `logical_size()`)
    /// to `Camera`, never `Window::width/height` or `pixel_width/height`.
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

    [[nodiscard]] TextureId store_texture(void* native_texture, int width, int height);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
