#pragma once

#include <midas/Renderer.hpp>
#include <midas/Texture.hpp>
#include <midas/Types.hpp>

namespace midas {

/// Position in world units and a uniform/non-uniform scale. No rotation yet —
/// draws stay axis-aligned so the math stays obvious.
///
/// This is a teachable 2D scene-graph *starter*: compose a child with
/// `parent.then(local)` instead of storing parent pointers (those dangle).
struct Transform {
    Vec2 position{};
    Vec2 scale{1.0f, 1.0f};

    [[nodiscard]] constexpr Rect to_rect(Vec2 size) const noexcept {
        return {position.x, position.y, size.x * scale.x, size.y * scale.y};
    }

    /// Map a point from this transform's local space into parent/world space.
    [[nodiscard]] constexpr Vec2 apply(Vec2 local_point) const noexcept {
        return position + local_point.hadamard(scale);
    }

    /// `parent.then(local)` — child's world transform when `local` is expressed
    /// in the parent's space. Scale is component-wise; there is no rotation.
    [[nodiscard]] constexpr Transform then(const Transform& local) const noexcept {
        return Transform{
            apply(local.position),
            scale.hadamard(local.scale),
        };
    }
};

/// One drawable object: a transform, an unscaled size, and either a solid fill
/// or a texture (tinted by `color`). This is not an ECS — just a bag of data
/// the sandbox (or a tiny game) can keep in an array.
///
/// `texture` is **non-owning**. The `Texture` must outlive the entity (in the
/// sandbox: load the texture first, then fill the entity list).
struct Entity {
    Transform transform;
    Vec2 size{};
    Color color{Color::white()};
    const Texture* texture{nullptr};

    [[nodiscard]] constexpr Rect bounds() const noexcept {
        return transform.to_rect(size);
    }

    [[nodiscard]] constexpr bool overlaps(const Entity& other) const noexcept {
        return bounds().overlaps(other.bounds());
    }
};

inline void draw_entity(Renderer& renderer, const Entity& entity) {
    const Rect dest = entity.bounds();
    // Negative scale is not supported yet (draws stay axis-aligned, positive size).
    if (dest.w <= 0.0f || dest.h <= 0.0f) {
        return;
    }
    if (entity.texture != nullptr) {
        renderer.draw_texture(*entity.texture, dest, entity.color);
    } else {
        renderer.fill_rect(dest, entity.color);
    }
}

}  // namespace midas
