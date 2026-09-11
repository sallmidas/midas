#pragma once

#include <midas/Types.hpp>

#include <algorithm>

namespace midas {

/// 2D orthographic view. `position` is the world point shown at the viewport center.
///
/// With `zoom == 1` and `position` at the viewport center, world units match
/// window / logical pixels (the default the renderer starts with).
struct Camera {
    Vec2 position{};
    float zoom{1.0f};

    static constexpr float min_zoom = 0.25f;
    static constexpr float max_zoom = 8.0f;

    [[nodiscard]] Vec2 world_to_screen(Vec2 world, float viewport_w, float viewport_h) const noexcept {
        const Vec2 half{viewport_w * 0.5f, viewport_h * 0.5f};
        return (world - position) * zoom + half;
    }

    [[nodiscard]] Vec2 screen_to_world(Vec2 screen, float viewport_w, float viewport_h) const noexcept {
        const Vec2 half{viewport_w * 0.5f, viewport_h * 0.5f};
        return (screen - half) / zoom + position;
    }

    [[nodiscard]] Rect project(const Rect& world, float viewport_w, float viewport_h) const noexcept {
        const Vec2 top_left = world_to_screen({world.x, world.y}, viewport_w, viewport_h);
        return {top_left.x, top_left.y, world.w * zoom, world.h * zoom};
    }

    void clamp_zoom() noexcept {
        zoom = std::clamp(zoom, min_zoom, max_zoom);
    }

    /// Multiply zoom, keeping `screen_point` over the same world point (cursor zoom).
    void zoom_toward(Vec2 screen_point, float multiplier, float viewport_w, float viewport_h) noexcept {
        const float next = std::clamp(zoom * multiplier, min_zoom, max_zoom);
        if (next == zoom) {
            return;
        }
        const Vec2 world = screen_to_world(screen_point, viewport_w, viewport_h);
        zoom = next;
        const Vec2 half{viewport_w * 0.5f, viewport_h * 0.5f};
        position = world - (screen_point - half) / zoom;
    }
};

}  // namespace midas
