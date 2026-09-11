#include <midas/Input.hpp>

#include "internal/Sdl.hpp"

#include <array>
#include <cstddef>

namespace midas {
namespace {

constexpr std::size_t key_count = static_cast<std::size_t>(Key::Down) + 1;

int index_of(Key key) noexcept {
    return static_cast<int>(key);
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

}  // namespace

struct Input::Impl {
    std::array<bool, key_count> down{};
    std::array<bool, key_count> previous{};
    bool quit{false};
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

void Input::begin_frame() noexcept {
    impl_->previous = impl_->down;
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
        default:
            break;
    }
}

void Input::request_quit() noexcept {
    impl_->quit = true;
}

}  // namespace midas
