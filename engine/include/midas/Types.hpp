#pragma once

#include <cmath>

namespace midas {

/// Inclusive clamp. NaN and values below `lo` become `lo`; above `hi` become `hi`.
/// If `lo` / `hi` are not a finite interval (`lo <= hi`), returns `lo`.
///
/// Prefer this over `std::clamp` for gameplay floats: `std::clamp` is undefined
/// when `lo > hi`, and NaN handling is implementation-defined.
[[nodiscard]] constexpr float clamp(float value, float lo, float hi) noexcept {
    if (!(lo <= hi)) {
        return lo;
    }
    if (!(value >= lo)) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

/// `clamp(value, 0, 1)`. NaN / negative → 0.
[[nodiscard]] constexpr float clamp01(float value) noexcept {
    return clamp(value, 0.0f, 1.0f);
}

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

    /// Mix `a` toward `b`. `t` is `clamp01`'d (NaN / negative → 0).
    /// Channels are not clamped — feed `gold()` / `white()` style 0–1 colors.
    [[nodiscard]] static constexpr Color lerp(Color a, Color b, float t) noexcept {
        const float u = clamp01(t);
        const float v = 1.0f - u;
        return {a.r * v + b.r * u, a.g * v + b.g * u, a.b * v + b.b * u, a.a * v + b.a * u};
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

    /// `x*x + y*y`. Use for length comparisons so you do not need `hypot`.
    /// `length()` stays the accurate magnitude (`std::hypot`).
    [[nodiscard]] constexpr float length_squared() const noexcept {
        return x * x + y * y;
    }

    /// `std::hypot(x, y)`. Prefer `length_squared()` when you only need to compare.
    [[nodiscard]] float length() const noexcept {
        return std::hypot(x, y);
    }

    /// Unit vector, or `{0, 0}` if the length is zero / non-finite (NaN, Inf).
    /// WASD pan uses this so a degenerate or NaN direction cannot fling the camera.
    [[nodiscard]] Vec2 normalized_or_zero() const noexcept {
        const float len = length();
        if (!std::isfinite(len) || len <= 0.0f) {
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
/// non-negative.
///
/// `contains` is half-open: `[x, x+w) × [y, y+h)`. `contains_inclusive` is
/// closed (`[x, x+w] × [y, y+h]`) — letterbox present bounds, where the far
/// edges are still the view. `overlaps` uses the same half-open edges as
/// `contains` — rectangles that only share a boundary do not overlap, and a
/// zero-size rect contains nothing and overlaps nothing.
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

    /// Closed test: `[x, x+w] × [y, y+h]`. SDL letterbox maps the drawable
    /// content onto that closed logical rect (far edges are still the view,
    /// not bars). Collision stays on half-open `contains`.
    [[nodiscard]] constexpr bool contains_inclusive(Vec2 point) const noexcept {
        return point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h;
    }

    [[nodiscard]] constexpr bool overlaps(const Rect& other) const noexcept {
        return x < other.x + other.w && x + w > other.x && y < other.y + other.h &&
               y + h > other.y;
    }

    /// Grow by `amount` on every edge (negative = shrink). A large inset can
    /// yield a non-positive size — that rect is empty (`overlaps` / `contains`
    /// fail). `inset(amount)` is `expanded(-amount)`. A NaN amount is a no-op
    /// (same idea as `clamp01(NaN)` → 0).
    [[nodiscard]] constexpr Rect expanded(float amount) const noexcept {
        if (!(amount >= 0.0f) && !(amount <= 0.0f)) {
            return *this;
        }
        return {x - amount, y - amount, w + amount * 2.0f, h + amount * 2.0f};
    }

    [[nodiscard]] constexpr Rect inset(float amount) const noexcept {
        return expanded(-amount);
    }
};

}  // namespace midas
