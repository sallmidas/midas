#pragma once

#include <midas/Types.hpp>

#include <memory>

namespace midas {

/// Gameplay keys. Add new enumerators at the end (`Input` sizes its array from
/// the last value) and update `map_key` / `sdl_keycode` / `kKeys` in Input.cpp.
enum class Key {
    Escape,
    Space,
    Enter,
    W,
    A,
    S,
    D,
    Q,
    E,
    Left,
    Right,
    Up,
    Down,
    F1,
    Grave,  // backtick / tilde key — keep last (array bound)
};

/// Add new buttons at the end (`Input` sizes its array from the last value).
enum class MouseButton {
    Left,
    Right,
    Middle,  // keep last (array bound)
};

/// Keyboard and mouse snapshot for the current tick.
///
/// `key_pressed` / `mouse_pressed` are edges (down this tick, up last tick).
/// Mouse position is in **logical present pixels** (the `EngineConfig` /
/// `Renderer::logical_size` space — same space as unzoomed world units when
/// the camera is at identity). The engine maps SDL **window coordinates**
/// through `SDL_RenderCoordinatesFromWindow` (letterbox + pixel density). Do
/// **not** multiply by `Window::pixel_density()` and do not pass
/// `Window::width` or pixel size to `Camera::zoom_toward`.
///
/// The drawable content maps onto the closed rect
/// `[0, logical_w] × [0, logical_h]` (far edges are still the view). Letterbox
/// bars yield a point outside that closed rect. Half-open `Rect::contains`
/// rejects the far edge — use `contains_inclusive` to skip bars without
/// dropping wheel zoom at the right/bottom of the present view.
///
/// `mouse_delta()` is zero until the first real sample, on focus gain, on
/// resize / DPI change, and while the window is unfocused, so a cursor warp
/// or letterbox remap cannot jump the camera. Per-tick mouse and wheel deltas
/// are also clamped — a huge OS jump still pans/zooms a bounded amount.
///
/// Focus loss releases held keys/buttons. Focus gain re-reads the OS keyboard
/// and mouse-button state so WASD still pans if you alt-tab back with a key
/// held (a leftover KEY_DOWN is not required). Held keys are copied into
/// `previous` so focus gain does not synthesize `key_pressed` / `mouse_pressed`
/// (that would quit on still-held Escape or toggle the HUD on still-held F1).
/// Key/button events while unfocused are ignored (sync on gain is the source
/// of truth). A window resize or display-scale / pixel-size change zeros mouse
/// delta so letterbox remapping cannot jump a right-drag pan.
class Input {
public:
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&&) = delete;
    ~Input();

    /// Held this tick (including keys restored on focus gain).
    [[nodiscard]] bool key_down(Key key) const noexcept;
    /// Edge: down this tick, up last tick. Not fired on focus gain.
    [[nodiscard]] bool key_pressed(Key key) const noexcept;
    [[nodiscard]] bool quit_requested() const noexcept;

    /// Logical present pixels (letterbox + DPI already applied).
    [[nodiscard]] Vec2 mouse_position() const noexcept;
    /// Logical delta this tick; 0 until the first sample / on focus / resize / unfocused.
    [[nodiscard]] Vec2 mouse_delta() const noexcept;
    /// This tick's wheel (clamped). 0 if unfocused.
    [[nodiscard]] float wheel_y() const noexcept;
    [[nodiscard]] bool mouse_down(MouseButton button) const noexcept;
    /// Edge: down this tick, up last tick. Not fired on focus gain.
    [[nodiscard]] bool mouse_pressed(MouseButton button) const noexcept;
    [[nodiscard]] bool window_focused() const noexcept;

private:
    friend class Engine;

    Input();

    void begin_frame() noexcept;
    void handle_native_event(const void* native_event) noexcept;
    void set_mouse_position(float x, float y) noexcept;
    void request_quit() noexcept;
    void release_held() noexcept;
    void sync_held_from_device() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
