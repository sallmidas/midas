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

    static constexpr Color white() noexcept {
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
};

struct Vec2 {
    float x{};
    float y{};

    constexpr Vec2 operator+(Vec2 other) const noexcept {
        return {x + other.x, y + other.y};
    }

    constexpr Vec2 operator-(Vec2 other) const noexcept {
        return {x - other.x, y - other.y};
    }

    constexpr Vec2 operator*(float scale) const noexcept {
        return {x * scale, y * scale};
    }

    constexpr Vec2 operator/(float scale) const noexcept {
        return {x / scale, y / scale};
    }

    constexpr Vec2& operator+=(Vec2 other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vec2& operator-=(Vec2 other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }
};

constexpr Vec2 operator*(float scale, Vec2 vec) noexcept {
    return vec * scale;
}

struct Rect {
    float x{};
    float y{};
    float w{};
    float h{};
};

}  // namespace midas
