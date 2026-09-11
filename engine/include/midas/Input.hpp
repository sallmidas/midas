#pragma once

#include <midas/Types.hpp>

#include <memory>

namespace midas {

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
    Grave,  // backtick / tilde key
};

enum class MouseButton {
    Left,
    Right,
    Middle,
};

/// Keyboard and mouse snapshot for the current tick.
///
/// `key_pressed` / `mouse_pressed` are edges (down this tick, up last tick).
/// Mouse position is in logical render coordinates (same space as unzoomed
/// world units when the camera is at identity). `mouse_delta()` is zero until
/// the first real sample, on focus gain, and while the window is unfocused, so
/// a cursor warp cannot jump the camera. Per-tick mouse and wheel deltas are
/// also clamped — a huge OS jump still pans/zooms a bounded amount.
///
/// Focus loss releases held keys/buttons. Focus gain re-reads the OS keyboard
/// and mouse-button state so WASD still pans if you alt-tab back with a key
/// held (a leftover KEY_DOWN is not required).
class Input {
public:
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&&) = delete;
    ~Input();

    [[nodiscard]] bool key_down(Key key) const noexcept;
    [[nodiscard]] bool key_pressed(Key key) const noexcept;
    [[nodiscard]] bool quit_requested() const noexcept;

    [[nodiscard]] Vec2 mouse_position() const noexcept;
    [[nodiscard]] Vec2 mouse_delta() const noexcept;
    [[nodiscard]] float wheel_y() const noexcept;
    [[nodiscard]] bool mouse_down(MouseButton button) const noexcept;
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
