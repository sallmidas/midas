#pragma once

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

}  // namespace midas
