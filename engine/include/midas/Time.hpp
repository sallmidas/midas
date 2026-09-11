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

    [[nodiscard]] double delta_seconds() const noexcept;
    [[nodiscard]] double elapsed_seconds() const noexcept;
    [[nodiscard]] double frame_seconds() const noexcept;
    [[nodiscard]] double frames_per_second() const noexcept;
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
/// ready. A non-finite `tick` is ignored.
struct Cooldown {
    float remaining{};

    void start(float seconds) noexcept {
        remaining = (std::isfinite(seconds) && seconds > 0.0f) ? seconds : 0.0f;
    }

    void tick(float dt) noexcept {
        if (!std::isfinite(dt) || dt <= 0.0f) {
            return;
        }
        remaining = clamp(remaining - dt, 0.0f, remaining);
    }

    [[nodiscard]] bool ready() const noexcept {
        return !(remaining > 0.0f);
    }
};

}  // namespace midas
