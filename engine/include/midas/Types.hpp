#pragma once

#include <cstdint>

namespace midas {

/// RGBA in 0–1. `gold()` / `charcoal()` are the Midas sandbox palette.
struct Color {
    float r{};
    float g{};
    float b{};
    float a{1.0f};

    static constexpr Color gold() noexcept {
        return {0.831f, 0.686f, 0.216f, 1.0f};
    }

    static constexpr Color charcoal() noexcept {
        return {0.10f, 0.09f, 0.08f, 1.0f};
    }
};

struct Rect {
    float x{};
    float y{};
    float w{};
    float h{};
};

}  // namespace midas
