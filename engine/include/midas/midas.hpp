#pragma once

/// Umbrella for the public API. Includes Engine, Window, Input, Time
/// (`Cooldown`), Renderer (`draw_texture` dest + optional atlas source rect),
/// Texture (`make_checkerboard_rgba`), Camera, Entity (`Transform`,
/// `draw_entity`, optional `source` cell), and Types (`Color`, `Vec2`, `Rect`,
/// `clamp` / `clamp01`).

#include <midas/Camera.hpp>
#include <midas/Engine.hpp>
#include <midas/Entity.hpp>
#include <midas/Input.hpp>
#include <midas/Renderer.hpp>
#include <midas/Texture.hpp>
#include <midas/Time.hpp>
#include <midas/Types.hpp>
#include <midas/Window.hpp>
