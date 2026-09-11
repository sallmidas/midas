#include <midas/Engine.hpp>

#include "internal/RendererNative.hpp"
#include "internal/Sdl.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
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
        if (config.max_ticks < 0) {
            config.max_ticks = 0;
        }
        time.reset();
    }

    EngineConfig config;
    bool running{false};
    // Construct: SDL → Window → Renderer. Destroy (reverse): Renderer →
    // Window → SDL_Quit. Drop game Textures before Engine; leftover Texture
    // dtors no-op after Renderer clears GpuLifetime::alive.
    SdlVideo sdl;
    Window window;
    Renderer renderer;
    Input input;
    Time time;
};

Engine::Engine(EngineConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {
    // Renderer already copied the created size as logical present. OS size
    // and pixel density may already differ (tiling WM, Retina); keep them as
    // separate numbers. Re-apply letterbox now so the high-DPI framebuffer
    // is mapped before the first tick (PIXEL_SIZE_CHANGED may still be queued).
    impl_->window.sync_size_from_native();
    impl_->renderer.reapply_logical_presentation();
}

Engine::~Engine() = default;  // Impl member order: Renderer, then Window, then SDL_Quit.

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

std::filesystem::path Engine::executable_directory() const {
    // SDL3 caches this string; do not SDL_free it.
    const char* base = SDL_GetBasePath();
    if (base == nullptr || base[0] == '\0') {
        return {};
    }
    return std::filesystem::path{base};
}

void Engine::pump_events() {
    auto* sdl_renderer = impl_->renderer.native() != nullptr ? impl_->renderer.native()->renderer : nullptr;

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
            event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
            impl_->window.sync_size_from_native();
            impl_->renderer.reapply_logical_presentation();
        }
        if (sdl_renderer != nullptr) {
            (void)SDL_ConvertEventToRenderCoordinates(sdl_renderer, &event);
        }
        impl_->input.handle_native_event(&event);
    }

    // SDL_GetMouseState is **window coordinates**, not logical present pixels
    // and not framebuffer pixels. On a 2× panel, window (640,360) is the
    // center of a 1280×720 client; the drawable may be 2560×1440. Feeding
    // those window coords (or pixels) straight to Camera::zoom_toward is
    // wrong whenever letterbox or pixel_density() is not 1:1.
    //
    // SDL_RenderCoordinatesFromWindow applies pixel density *and* logical
    // letterbox. If it fails, keep the last logical sample — do not fall
    // back to window coords (that is the classic high-DPI / resize bug).
    if (sdl_renderer != nullptr) {
        float window_x = 0.0f;
        float window_y = 0.0f;
        (void)SDL_GetMouseState(&window_x, &window_y);
        float logical_x = 0.0f;
        float logical_y = 0.0f;
        if (SDL_RenderCoordinatesFromWindow(sdl_renderer, window_x, window_y, &logical_x,
                                            &logical_y) &&
            std::isfinite(logical_x) && std::isfinite(logical_y)) {
            impl_->input.set_mouse_position(logical_x, logical_y);
        }
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
        // Wall time of this tick including the 60 Hz wait (or the overrun).
        // Gameplay still uses Time::delta_seconds() == 1/60; HUD uses this.
        impl_->time.complete_frame(SDL_GetTicksNS() - frame_start);
    }

    return 0;
}

}  // namespace midas
