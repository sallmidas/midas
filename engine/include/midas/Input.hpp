#pragma once

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
    Left,
    Right,
    Up,
    Down,
};

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

private:
    friend class Engine;

    Input();

    void begin_frame() noexcept;
    void handle_native_event(const void* native_event) noexcept;
    void request_quit() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
