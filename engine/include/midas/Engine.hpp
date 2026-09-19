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
    /// 0 = run until quit. Positive = stop after that many 60 Hz **simulation**
    /// ticks (`on_update` calls), not display presents. Sandbox `--smoke` /
    /// `MIDAS_SMOKE_FRAMES` uses this. Room `--smoke` does too.
    int max_ticks{0};
};

/// Owns the four engine modules (`Window`, `Input`, `Time`, `Renderer`) and
/// the accumulator frame loop (fixed 60 Hz simulation, display presents separately).
///
/// Copies/moves are deleted because the SDL video subsystem and native window are
/// unique. Pass update + present callbacks to `run()`; call `request_quit()`
/// (or press Esc in the sandbox) to stop.
///
/// **Shutdown** (C++ destroys members in reverse declaration order):
/// `Renderer` (GPU textures it owns, then `SDL_DestroyRenderer`) then `Window`
/// then `SDL_Quit`. Games hold `TextureId` copies, not owning texture pointers.
/// Destroying this `Engine` invalidates every id the renderer issued.
/// `draw_texture` / `draw_entity` skip an invalid or stale id (no crash).
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

    /// Directory of the running executable (`SDL_GetBasePath`). Assets copied
    /// next to the binary live in `executable_directory() / "assets"`.
    /// Empty if SDL cannot report a path.
    [[nodiscard]] std::filesystem::path executable_directory() const;

    void request_quit() noexcept;
    [[nodiscard]] bool is_running() const noexcept;

    /// Max simulation ticks per display frame. A hitch longer than this many
    /// ticks is clamped before it enters the accumulator (no death spiral).
    /// The sub-tick remainder stays in the accumulator for the next frame.
    static constexpr int max_catch_up = 4;

    /// Accumulator loop: pump OS events once per display frame, run 0..`max_catch_up`
    /// simulation ticks (`on_update`, each with `Time::delta_seconds() == 1/60`),
    /// then `on_present` once. Sleeps the leftover of the 1/60 s display budget
    /// when ahead of 60 Hz (dummy video has no vsync). Stops on `request_quit`,
    /// close box, or `EngineConfig::max_ticks` (simulation ticks).
    ///
    /// Construction of `Engine` / `create_texture` / `load_bmp` may throw. Callbacks must not: an
    /// in-tick load failure should skip or return an error, not unwind `run()`.
    /// Returns 0 on a normal exit (an SDL pump/present failure still throws).
    int run(std::function<void(Engine&)> on_update, std::function<void(Engine&)> on_present);

private:
    void pump_events();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
