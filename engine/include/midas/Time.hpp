#pragma once

#include <cstdint>
#include <memory>

namespace midas {

/// Fixed 60 Hz timestep. `delta_seconds()` is always `1/60` while the loop is capped.
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
    [[nodiscard]] std::uint64_t tick_index() const noexcept;

private:
    friend class Engine;

    Time();

    void reset() noexcept;
    void advance_tick() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
