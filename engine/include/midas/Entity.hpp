#pragma once

#include <midas/Renderer.hpp>
#include <midas/Texture.hpp>
#include <midas/Types.hpp>

namespace midas {

/// Position in world units and a uniform/non-uniform scale. No rotation yet —
/// draws stay axis-aligned so the math stays obvious.
struct Transform {
    Vec2 position{};
    Vec2 scale{1.0f, 1.0f};

    [[nodiscard]] constexpr Rect to_rect(Vec2 size) const noexcept {
        return {position.x, position.y, size.x * scale.x, size.y * scale.y};
    }
};

/// One drawable object: a transform, an unscaled size, and either a solid fill
/// or a texture (tinted by `color`). This is not an ECS — just a bag of data
/// the sandbox (or a tiny game) can keep in an array.
struct Entity {
    Transform transform;
    Vec2 size{};
    Color color{Color::white()};
    const Texture* texture{nullptr};

    [[nodiscard]] constexpr Rect bounds() const noexcept {
        return transform.to_rect(size);
    }
};

inline void draw_entity(Renderer& renderer, const Entity& entity) {
    const Rect dest = entity.bounds();
    if (entity.texture != nullptr) {
        renderer.draw_texture(*entity.texture, dest, entity.color);
    } else {
        renderer.fill_rect(dest, entity.color);
    }
}

}  // namespace midas
