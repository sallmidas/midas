#pragma once

#include <cmath>
#include <cstdint>

namespace midas {

/// RGBA in 0–1. `gold()` / `bronze()` / `charcoal()` are the Midas sandbox palette.
struct Color {
    float r{};
    float g{};
    float b{};
    float a{1.0f};

    static constexpr Color gold() noexcept {
        return {0.831f, 0.686f, 0.216f, 1.0f};
    }

    static constexpr Color bronze() noexcept {
        return {0.45f, 0.32f, 0.10f, 1.0f};
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

    /// Component-wise scale (parent * local in a 2D scene graph without rotation).
    [[nodiscard]] constexpr Vec2 hadamard(Vec2 other) const noexcept {
        return {x * other.x, y * other.y};
    }

    [[nodiscard]] float length() const noexcept {
        return std::hypot(x, y);
    }

    [[nodiscard]] Vec2 normalized_or_zero() const noexcept {
        const float len = length();
        if (len <= 0.0f) {
            return {};
        }
        return *this / len;
    }
};

constexpr Vec2 operator*(float scale, Vec2 vec) noexcept {
    return vec * scale;
}

/// Axis-aligned rectangle. For 2D Midas, this *is* an AABB: origin at the
/// top-left, `w`/`h` extending right and down (same as SDL). Keep `w` and `h`
/// non-negative; `overlaps` / `contains` assume that.
struct Rect {
    float x{};
    float y{};
    float w{};
    float h{};

    [[nodiscard]] constexpr Vec2 position() const noexcept {
        return {x, y};
    }

    [[nodiscard]] constexpr Vec2 size() const noexcept {
        return {w, h};
    }

    [[nodiscard]] constexpr bool contains(Vec2 point) const noexcept {
        return point.x >= x && point.x < x + w && point.y >= y && point.y < y + h;
    }

    [[nodiscard]] constexpr bool overlaps(const Rect& other) const noexcept {
        return x < other.x + other.w && x + w > other.x && y < other.y + other.h &&
               y + h > other.y;
    }
};

}  // namespace midas
