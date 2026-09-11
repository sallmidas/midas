#pragma once

#include <midas/Types.hpp>

#include <cmath>
#include <cstdint>
#include <memory>

namespace midas {

/// Fixed 60 Hz timestep.
///
/// Use `delta_seconds()` (always `1/60` while the loop is capped) for gameplay
/// such as camera pan. A hitch does not fling the camera — the next tick still
/// moves as if 16.6 ms passed. `elapsed_seconds()` is wall-clock time since
/// `run()` started. `frame_seconds()` / `frames_per_second()` are the previous
/// completed tick's wall time (work + 60 Hz sleep, or the overrun if a tick
/// ran long) — HUD / pacing, not motion.
class Time {
public:
    static constexpr int tick_hz = 60;
    static constexpr double tick_seconds = 1.0 / 60.0;

    Time(const Time&) = delete;
    Time& operator=(const Time&) = delete;
    Time(Time&&) = delete;
    Time& operator=(Time&&) = delete;
    ~Time();

    /// Always `tick_seconds` (1/60). Use for gameplay motion, including `Cooldown`.
    [[nodiscard]] double delta_seconds() const noexcept;
    /// Wall-clock seconds since `run()` started (0 before `run`).
    [[nodiscard]] double elapsed_seconds() const noexcept;
    /// Wall time of the previous completed tick (work + 60 Hz sleep). HUD, not motion.
    [[nodiscard]] double frame_seconds() const noexcept;
    /// `1 / frame_seconds()`, or 0 if no tick has completed yet / non-finite.
    [[nodiscard]] double frames_per_second() const noexcept;
    /// Ticks completed in the current `run()` (0 after `reset`).
    [[nodiscard]] std::uint64_t tick_index() const noexcept;

private:
    friend class Engine;

    Time();

    void reset() noexcept;
    void advance_tick() noexcept;
    void complete_frame(std::uint64_t frame_ns) noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Seconds remaining until an action is allowed again.
///
/// Tick with `Time::delta_seconds()` (the fixed 1/60 gameplay step, not
/// wall-clock `frame_seconds()`). Default-constructed and expired cooldowns
/// are `ready()`. `start` of a non-positive or non-finite duration leaves it
/// ready. A non-finite `tick` is ignored. Non-finite `remaining` (a direct
/// write) is expired: `ready()` is true and `tick` snaps it to 0 — same
/// policy as a bad `start`.
struct Cooldown {
    float remaining{};

    void start(float seconds) noexcept {
        remaining = (std::isfinite(seconds) && seconds > 0.0f) ? seconds : 0.0f;
    }

    void tick(float dt) noexcept {
        if (!std::isfinite(remaining) || remaining <= 0.0f) {
            remaining = 0.0f;
            return;
        }
        if (!std::isfinite(dt) || dt <= 0.0f) {
            return;
        }
        remaining = clamp(remaining - dt, 0.0f, remaining);
    }

    [[nodiscard]] bool ready() const noexcept {
        return !(std::isfinite(remaining) && remaining > 0.0f);
    }
};

}  // namespace midas
