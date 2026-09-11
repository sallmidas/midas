#include <midas/Engine.hpp>

#include "internal/Sdl.hpp"

#include <cstdint>
#include <utility>

namespace midas {
namespace {

struct SdlVideo {
    SdlVideo() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            detail::throw_sdl("SDL_Init failed");
        }
    }

    SdlVideo(const SdlVideo&) = delete;
    SdlVideo& operator=(const SdlVideo&) = delete;

    ~SdlVideo() { SDL_Quit(); }
};

}  // namespace

struct Engine::Impl {
    explicit Impl(EngineConfig cfg)
        : config(std::move(cfg)),
          sdl(),
          window(config.title, config.width, config.height),
          renderer(window) {
        time.reset();
    }

    EngineConfig config;
    bool running{false};
    SdlVideo sdl;
    Window window;
    Renderer renderer;
    Input input;
    Time time;
};

Engine::Engine(EngineConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

Engine::~Engine() = default;

Window& Engine::window() noexcept {
    return impl_->window;
}

const Window& Engine::window() const noexcept {
    return impl_->window;
}

Input& Engine::input() noexcept {
    return impl_->input;
}

const Input& Engine::input() const noexcept {
    return impl_->input;
}

Time& Engine::time() noexcept {
    return impl_->time;
}

const Time& Engine::time() const noexcept {
    return impl_->time;
}

Renderer& Engine::renderer() noexcept {
    return impl_->renderer;
}

const Renderer& Engine::renderer() const noexcept {
    return impl_->renderer;
}

void Engine::request_quit() noexcept {
    impl_->running = false;
    impl_->input.request_quit();
}

bool Engine::is_running() const noexcept {
    return impl_->running;
}

void Engine::pump_events() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        impl_->input.handle_native_event(&event);
    }
}

int Engine::run(std::function<void(Engine&)> on_tick) {
    impl_->running = true;
    impl_->time.reset();

    constexpr std::uint64_t tick_ns = 1'000'000'000 / static_cast<std::uint64_t>(Time::tick_hz);

    while (impl_->running) {
        const auto frame_start = SDL_GetTicksNS();

        impl_->input.begin_frame();
        pump_events();

        if (impl_->input.quit_requested()) {
            impl_->running = false;
            break;
        }

        if (on_tick) {
            on_tick(*this);
        }

        impl_->time.advance_tick();
        if (impl_->config.max_ticks > 0 &&
            impl_->time.tick_index() >= static_cast<std::uint64_t>(impl_->config.max_ticks)) {
            impl_->running = false;
        }

        const auto elapsed = SDL_GetTicksNS() - frame_start;
        if (elapsed < tick_ns) {
            SDL_DelayNS(tick_ns - elapsed);
        }
    }

    return 0;
}

}  // namespace midas
