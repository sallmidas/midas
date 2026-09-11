#include <midas/Time.hpp>

#include "internal/Sdl.hpp"

#include <cmath>

namespace midas {

struct Time::Impl {
    std::uint64_t start_ns{0};
    std::uint64_t tick{0};
    double frame_seconds{0.0};
};

Time::Time() : impl_(std::make_unique<Impl>()) {}

Time::~Time() = default;

double Time::delta_seconds() const noexcept {
    return tick_seconds;
}

double Time::elapsed_seconds() const noexcept {
    if (impl_->start_ns == 0) {
        return 0.0;
    }
    const auto now = SDL_GetTicksNS();
    return static_cast<double>(now - impl_->start_ns) / 1'000'000'000.0;
}

double Time::frame_seconds() const noexcept {
    return impl_->frame_seconds;
}

double Time::frames_per_second() const noexcept {
    if (!(impl_->frame_seconds > 0.0) || !std::isfinite(impl_->frame_seconds)) {
        return 0.0;
    }
    return 1.0 / impl_->frame_seconds;
}

std::uint64_t Time::tick_index() const noexcept {
    return impl_->tick;
}

void Time::reset() noexcept {
    impl_->start_ns = SDL_GetTicksNS();
    impl_->tick = 0;
    impl_->frame_seconds = 0.0;
}

void Time::advance_tick() noexcept {
    ++impl_->tick;
}

void Time::complete_frame(std::uint64_t frame_ns) noexcept {
    impl_->frame_seconds = static_cast<double>(frame_ns) / 1'000'000'000.0;
}

}  // namespace midas
