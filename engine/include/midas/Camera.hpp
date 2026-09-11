#pragma once

#include <midas/Types.hpp>

#include <algorithm>
#include <cmath>

namespace midas {

/// 2D orthographic view. `position` is the world point shown at the viewport center.
///
/// Zoom is a **uniform** scale, so the visible world size is
/// `(viewport_w / zoom)` by `(viewport_h / zoom)`. That keeps the view
/// aspect-correct: it matches the logical window aspect, and circles stay
/// circles. Letterboxing (see `Renderer`) handles non-matching window pixels.
///
/// With `zoom == 1` and `position` at the viewport center, world units match
/// window / logical pixels (the default the renderer starts with).
///
/// Zoom is kept in `[min_zoom, max_zoom]`. Projection uses a sanitized zoom so
/// a zero/NaN zoom cannot divide by zero; call `sanitize()` after writing
/// `zoom` or `position` yourself.
struct Camera {
    Vec2 position{};
    float zoom{1.0f};

    static constexpr float min_zoom = 0.25f;
    static constexpr float max_zoom = 8.0f;

    /// Zoom used for projection: non-finite or non-positive values become 1,
    /// otherwise clamped to `[min_zoom, max_zoom]`.
    [[nodiscard]] float clamped_zoom() const noexcept {
        if (!std::isfinite(zoom) || zoom <= 0.0f) {
            return 1.0f;
        }
        return std::clamp(zoom, min_zoom, max_zoom);
    }

    void clamp_zoom() noexcept {
        zoom = clamped_zoom();
    }

    /// Repair `zoom` and drop non-finite pan. Safe to call every tick.
    void sanitize() noexcept {
        clamp_zoom();
        if (!std::isfinite(position.x)) {
            position.x = 0.0f;
        }
        if (!std::isfinite(position.y)) {
            position.y = 0.0f;
        }
    }

    [[nodiscard]] Vec2 world_to_screen(Vec2 world, float viewport_w, float viewport_h) const noexcept {
        const float z = clamped_zoom();
        const Vec2 half{viewport_w * 0.5f, viewport_h * 0.5f};
        return (world - position) * z + half;
    }

    [[nodiscard]] Vec2 screen_to_world(Vec2 screen, float viewport_w, float viewport_h) const noexcept {
        const float z = clamped_zoom();
        const Vec2 half{viewport_w * 0.5f, viewport_h * 0.5f};
        return (screen - half) / z + position;
    }

    [[nodiscard]] Rect project(const Rect& world, float viewport_w, float viewport_h) const noexcept {
        const float z = clamped_zoom();
        const Vec2 top_left = world_to_screen({world.x, world.y}, viewport_w, viewport_h);
        return {top_left.x, top_left.y, world.w * z, world.h * z};
    }

    /// World rectangle currently visible in the logical viewport. Width/height
    /// ratio equals `viewport_w / viewport_h` (aspect-correct ortho).
    [[nodiscard]] Rect visible_world_rect(float viewport_w, float viewport_h) const noexcept {
        const Vec2 top_left = screen_to_world({0.0f, 0.0f}, viewport_w, viewport_h);
        const float z = clamped_zoom();
        return {top_left.x, top_left.y, viewport_w / z, viewport_h / z};
    }

    /// Multiply zoom, keeping `screen_point` over the same world point (cursor zoom).
    /// `multiplier` must be finite and positive; values that would exceed the
    /// zoom limits are clamped (the view stops, it does not wrap).
    void zoom_toward(Vec2 screen_point, float multiplier, float viewport_w, float viewport_h) noexcept {
        sanitize();
        if (!std::isfinite(multiplier) || multiplier <= 0.0f) {
            return;
        }

        const float next = std::clamp(zoom * multiplier, min_zoom, max_zoom);
        if (next == zoom) {
            return;
        }

        const Vec2 world = screen_to_world(screen_point, viewport_w, viewport_h);
        zoom = next;
        const Vec2 half{viewport_w * 0.5f, viewport_h * 0.5f};
        position = world - (screen_point - half) / zoom;
        sanitize();
    }
};

}  // namespace midas
