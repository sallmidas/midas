#include <midas/Input.hpp>

#include "internal/Sdl.hpp"

#include <array>
#include <cstddef>

namespace midas {
namespace {

constexpr std::size_t key_count = static_cast<std::size_t>(Key::Down) + 1;
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
        default:
            return false;
    }
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
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (event.key.repeat) {
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
            MouseButton button{};
            if (!map_mouse_button(event.button.button, button)) {
                break;
            }
            impl_->mouse_down[static_cast<std::size_t>(index_of(button))] = event.button.down;
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            impl_->wheel_y += event.wheel.y;
            break;
        default:
            break;
    }
}

void Input::set_mouse_position(float x, float y) noexcept {
    impl_->mouse_x = x;
    impl_->mouse_y = y;
    if (!impl_->mouse_initialized) {
        impl_->prev_mouse_x = x;
        impl_->prev_mouse_y = y;
        impl_->mouse_dx = 0.0f;
        impl_->mouse_dy = 0.0f;
        impl_->mouse_initialized = true;
        return;
    }
    impl_->mouse_dx = x - impl_->prev_mouse_x;
    impl_->mouse_dy = y - impl_->prev_mouse_y;
}

void Input::request_quit() noexcept {
    impl_->quit = true;
}

}  // namespace midas
