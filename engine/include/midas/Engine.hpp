#pragma once

#include <midas/Input.hpp>
#include <midas/Renderer.hpp>
#include <midas/Time.hpp>
#include <midas/Window.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace midas {

struct EngineConfig {
    std::string title{"Midas"};
    int width{1280};
    int height{720};
    /// 0 = run until quit. Positive = stop after that many 60 Hz ticks (sandbox `--smoke`).
    int max_ticks{0};
};

/// Owns the four engine modules (`Window`, `Input`, `Time`, `Renderer`) and
/// the 60 Hz tick loop.
///
/// Copies/moves are deleted because the SDL video subsystem and native window are
/// unique. Pass a tick callback to `run()`; call `request_quit()` (or press Esc
/// in the sandbox) to stop.
///
/// **Shutdown** (C++ destroys members in reverse declaration order):
/// `Renderer` then `Window` then `SDL_Quit`. Destroy game `Texture`s before
/// this `Engine`. If a texture outlives the renderer, its destructor is a
/// no-op on the GPU handle (the renderer shares a small alive-flag; each
/// `Texture` holds a `shared_ptr` to that flag — `shared_ptr<void>` at the
/// private constructor so `Texture.hpp` stays SDL-free).
///
/// TODO: 2D audio (SDL3 audio device) is a later module; this loop is still
/// video + input.
class Engine {
public:
    explicit Engine(EngineConfig config = {});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    [[nodiscard]] Window& window() noexcept;
    [[nodiscard]] const Window& window() const noexcept;
    [[nodiscard]] Input& input() noexcept;
    [[nodiscard]] const Input& input() const noexcept;
    [[nodiscard]] Time& time() noexcept;
    [[nodiscard]] const Time& time() const noexcept;
    [[nodiscard]] Renderer& renderer() noexcept;
    [[nodiscard]] const Renderer& renderer() const noexcept;

    /// Directory of the running executable (`SDL_GetBasePath`). Sandbox assets
    /// copied next to the binary live in `executable_directory() / "assets"`.
    /// Empty if SDL cannot report a path.
    [[nodiscard]] std::filesystem::path executable_directory() const;

    void request_quit() noexcept;
    [[nodiscard]] bool is_running() const noexcept;

    /// 60 Hz loop: pump events, `on_tick`, sleep the remainder of 1/60 s.
    /// Stops on `request_quit`, close box, or `EngineConfig::max_ticks`.
    /// Returns 0 on a normal exit (throws if a tick or SDL call fails).
    int run(std::function<void(Engine&)> on_tick);

private:
    void pump_events();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
