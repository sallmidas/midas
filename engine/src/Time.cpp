#include <midas/Time.hpp>

#include "internal/Sdl.hpp"

namespace midas {

struct Time::Impl {
    std::uint64_t start_ns{0};
    std::uint64_t tick{0};
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

std::uint64_t Time::tick_index() const noexcept {
    return impl_->tick;
}

void Time::reset() noexcept {
    impl_->start_ns = SDL_GetTicksNS();
    impl_->tick = 0;
}

void Time::advance_tick() noexcept {
    ++impl_->tick;
}

}  // namespace midas
