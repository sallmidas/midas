#include <midas/Input.hpp>

#include "internal/Sdl.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace midas {
namespace {

constexpr std::size_t key_count = static_cast<std::size_t>(Key::Grave) + 1;
constexpr std::size_t mouse_button_count = static_cast<std::size_t>(MouseButton::Middle) + 1;

int index_of(Key key) noexcept {
    return static_cast<int>(key);
}

int index_of(MouseButton button) noexcept {
    return static_cast<int>(button);
}

bool map_key(SDL_Keycode code, Key& out) noexcept {
    switch (code) {
        case SDLK_ESCAPE:
            out = Key::Escape;
            return true;
        case SDLK_SPACE:
            out = Key::Space;
            return true;
        case SDLK_RETURN:
            out = Key::Enter;
            return true;
        case SDLK_W:
            out = Key::W;
            return true;
        case SDLK_A:
            out = Key::A;
            return true;
        case SDLK_S:
            out = Key::S;
            return true;
        case SDLK_D:
            out = Key::D;
            return true;
        case SDLK_Q:
            out = Key::Q;
            return true;
        case SDLK_E:
            out = Key::E;
            return true;
        case SDLK_LEFT:
            out = Key::Left;
            return true;
        case SDLK_RIGHT:
            out = Key::Right;
            return true;
        case SDLK_UP:
            out = Key::Up;
            return true;
        case SDLK_DOWN:
            out = Key::Down;
            return true;
        case SDLK_F1:
            out = Key::F1;
            return true;
        case SDLK_GRAVE:
            out = Key::Grave;
            return true;
        default:
            return false;
    }
}

SDL_Keycode sdl_keycode(Key key) noexcept {
    switch (key) {
        case Key::Escape:
            return SDLK_ESCAPE;
        case Key::Space:
            return SDLK_SPACE;
        case Key::Enter:
            return SDLK_RETURN;
        case Key::W:
            return SDLK_W;
        case Key::A:
            return SDLK_A;
        case Key::S:
            return SDLK_S;
        case Key::D:
            return SDLK_D;
        case Key::Q:
            return SDLK_Q;
        case Key::E:
            return SDLK_E;
        case Key::Left:
            return SDLK_LEFT;
        case Key::Right:
            return SDLK_RIGHT;
        case Key::Up:
            return SDLK_UP;
        case Key::Down:
            return SDLK_DOWN;
        case Key::F1:
            return SDLK_F1;
        case Key::Grave:
            return SDLK_GRAVE;
    }
    return SDLK_UNKNOWN;
}

bool map_mouse_button(Uint8 button, MouseButton& out) noexcept {
    switch (button) {
        case SDL_BUTTON_LEFT:
            out = MouseButton::Left;
            return true;
        case SDL_BUTTON_RIGHT:
            out = MouseButton::Right;
            return true;
        case SDL_BUTTON_MIDDLE:
            out = MouseButton::Middle;
            return true;
        default:
            return false;
    }
}

constexpr float kMaxMouseDelta = 512.0f;
constexpr float kMaxWheelY = 8.0f;

}  // namespace

struct Input::Impl {
    std::array<bool, key_count> down{};
    std::array<bool, key_count> previous{};
    std::array<bool, mouse_button_count> mouse_down{};
    std::array<bool, mouse_button_count> mouse_previous{};
    float mouse_x{};
    float mouse_y{};
    float prev_mouse_x{};
    float prev_mouse_y{};
    float mouse_dx{};
    float mouse_dy{};
    float wheel_y{};
    bool quit{false};
    bool mouse_initialized{false};
    bool focused{true};
};

Input::Input() : impl_(std::make_unique<Impl>()) {}

Input::~Input() = default;

bool Input::key_down(Key key) const noexcept {
    const int index = index_of(key);
    if (index < 0 || index >= static_cast<int>(key_count)) {
        return false;
    }
    return impl_->down[static_cast<std::size_t>(index)];
}

bool Input::key_pressed(Key key) const noexcept {
    const int index = index_of(key);
    if (index < 0 || index >= static_cast<int>(key_count)) {
        return false;
    }
    const auto i = static_cast<std::size_t>(index);
    return impl_->down[i] && !impl_->previous[i];
}

bool Input::quit_requested() const noexcept {
    return impl_->quit;
}

Vec2 Input::mouse_position() const noexcept {
    return {impl_->mouse_x, impl_->mouse_y};
}

Vec2 Input::mouse_delta() const noexcept {
    return {impl_->mouse_dx, impl_->mouse_dy};
}

float Input::wheel_y() const noexcept {
    return impl_->wheel_y;
}

bool Input::mouse_down(MouseButton button) const noexcept {
    const int index = index_of(button);
    if (index < 0 || index >= static_cast<int>(mouse_button_count)) {
        return false;
    }
    return impl_->mouse_down[static_cast<std::size_t>(index)];
}

bool Input::mouse_pressed(MouseButton button) const noexcept {
    const int index = index_of(button);
    if (index < 0 || index >= static_cast<int>(mouse_button_count)) {
        return false;
    }
    const auto i = static_cast<std::size_t>(index);
    return impl_->mouse_down[i] && !impl_->mouse_previous[i];
}

bool Input::window_focused() const noexcept {
    return impl_->focused;
}

void Input::begin_frame() noexcept {
    impl_->previous = impl_->down;
    impl_->mouse_previous = impl_->mouse_down;
    impl_->prev_mouse_x = impl_->mouse_x;
    impl_->prev_mouse_y = impl_->mouse_y;
    impl_->mouse_dx = 0.0f;
    impl_->mouse_dy = 0.0f;
    impl_->wheel_y = 0.0f;
}

void Input::handle_native_event(const void* native_event) noexcept {
    if (native_event == nullptr) {
        return;
    }

    const auto& event = *static_cast<const SDL_Event*>(native_event);
    switch (event.type) {
        case SDL_EVENT_QUIT:
            impl_->quit = true;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            release_held();
            impl_->focused = false;
            impl_->mouse_initialized = false;
            impl_->mouse_dx = 0.0f;
            impl_->mouse_dy = 0.0f;
            impl_->wheel_y = 0.0f;
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            impl_->focused = true;
            impl_->mouse_initialized = false;
            impl_->mouse_dx = 0.0f;
            impl_->mouse_dy = 0.0f;
            // Pass-2 released keys on focus loss, so a held WASD would stay
            // dead until a new KEY_DOWN. Re-read the OS so pan resumes.
            sync_held_from_device();
            // Same-frame `key_pressed` uses `down && !previous`. Copy the
            // restored holds into previous so Escape/F1/Space do not fire.
            impl_->previous = impl_->down;
            impl_->mouse_previous = impl_->mouse_down;
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            bool held = false;
            for (bool down : impl_->mouse_down) {
                if (down) {
                    held = true;
                    break;
                }
            }
            // Keep tracking while a button is held so right-drag pan can leave the window.
            // Otherwise the next enter would see a huge delta from the last interior sample.
            if (!held) {
                impl_->mouse_initialized = false;
                impl_->mouse_dx = 0.0f;
                impl_->mouse_dy = 0.0f;
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (event.key.repeat) {
                break;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !impl_->focused) {
                break;
            }
            Key key{};
            if (!map_key(event.key.key, key)) {
                break;
            }
            impl_->down[static_cast<std::size_t>(index_of(key))] = event.key.down;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !impl_->focused) {
                break;
            }
            MouseButton button{};
            if (!map_mouse_button(event.button.button, button)) {
                break;
            }
            impl_->mouse_down[static_cast<std::size_t>(index_of(button))] = event.button.down;
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            if (!impl_->focused || !std::isfinite(event.wheel.y)) {
                break;
            }
            impl_->wheel_y = std::clamp(impl_->wheel_y + event.wheel.y, -kMaxWheelY, kMaxWheelY);
            break;
        default:
            break;
    }
}

void Input::set_mouse_position(float x, float y) noexcept {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        impl_->mouse_dx = 0.0f;
        impl_->mouse_dy = 0.0f;
        return;
    }

    impl_->mouse_x = x;
    impl_->mouse_y = y;
    if (!impl_->focused || !impl_->mouse_initialized) {
        impl_->prev_mouse_x = x;
        impl_->prev_mouse_y = y;
        impl_->mouse_dx = 0.0f;
        impl_->mouse_dy = 0.0f;
        if (impl_->focused) {
            impl_->mouse_initialized = true;
        }
        return;
    }
    impl_->mouse_dx = std::clamp(x - impl_->prev_mouse_x, -kMaxMouseDelta, kMaxMouseDelta);
    impl_->mouse_dy = std::clamp(y - impl_->prev_mouse_y, -kMaxMouseDelta, kMaxMouseDelta);
}

void Input::request_quit() noexcept {
    impl_->quit = true;
}

void Input::release_held() noexcept {
    impl_->down.fill(false);
    impl_->mouse_down.fill(false);
}

void Input::sync_held_from_device() noexcept {
    int numkeys = 0;
    const bool* state = SDL_GetKeyboardState(&numkeys);
    if (state != nullptr && numkeys > 0) {
        static constexpr Key kKeys[] = {
            Key::Escape, Key::Space, Key::Enter, Key::W, Key::A, Key::S, Key::D,
            Key::Q,      Key::E,     Key::Left,  Key::Right, Key::Up,   Key::Down,
            Key::F1,     Key::Grave,
        };
        for (Key key : kKeys) {
            const SDL_Scancode scancode = SDL_GetScancodeFromKey(sdl_keycode(key), nullptr);
            const int index = static_cast<int>(scancode);
            const bool held = index >= 0 && index < numkeys && state[index];
            impl_->down[static_cast<std::size_t>(index_of(key))] = held;
        }
    }

    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    impl_->mouse_down[static_cast<std::size_t>(index_of(MouseButton::Left))] =
        (buttons & SDL_BUTTON_LMASK) != 0;
    impl_->mouse_down[static_cast<std::size_t>(index_of(MouseButton::Right))] =
        (buttons & SDL_BUTTON_RMASK) != 0;
    impl_->mouse_down[static_cast<std::size_t>(index_of(MouseButton::Middle))] =
        (buttons & SDL_BUTTON_MMASK) != 0;
}

}  // namespace midas
