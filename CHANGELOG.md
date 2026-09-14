# Changelog

Bullet highlights from the 2D engine fine-tunes on `main`. No 3D / ECS / editor.

## Unreleased (accumulator timestep)

- `Engine::run(on_update, on_present)` replaces the single `on_tick` callback. Simulation is fixed at 60 Hz (`Time::tick_hz`); present runs once per display frame after 0..`Engine::max_catch_up` (4) catch-up ticks.
- Real-time accumulator on `Engine::Impl`: frame elapsed is added, leftover stays, a hitch longer than 4 ticks is clamped (no death spiral). Dummy video still sleeps the remainder when ahead of 60 Hz.
- Sandbox: camera / HUD toggle / plinth pulse in `on_update`; clear / draw / `present` in `on_present`.
- `--smoke` / `MIDAS_SMOKE_FRAMES` still count **simulation ticks** (`EngineConfig::max_ticks`), not presents.
- Docs: ARCHITECTURE frame-loop + locked policies (construction may throw; in-tick loads must not unwind `run()`; `SDL_Renderer` is the host).

## Unreleased (sprite atlas)

- `Renderer::draw_texture` accepts a texture-space source `Rect` (atlas cell) in addition to the world dest and optional tint. The existing three-argument call still draws the whole texture (`src == {}` → SDL `nullptr`).
- `Entity::source` is an optional cell rect; empty / non-positive size keeps full-texture draws. `draw_entity` forwards a positive source to the new overload.
- Sandbox uploads one 64×32 two-cell sheet (`make_checkerboard_rgba` + `create_texture`) and draws the gold cell and the bronze cell side by side.
- Docs: ARCHITECTURE / README / BUILDING note source rects and the two-cell sandbox demo.

## Unreleased (pass 15)

- CHANGELOG records pass 14 as landed on `main`.
- BUILDING: one-paragraph “How to run on Saul’s Mac” (Homebrew `sdl3`, `cmake --preset debug`, `midas_sandbox`).
- Startup-log docs match the printed `logical / window / pixels / density` line (leftover “three sizes” wording). Engine mouse-mapping comment uses `pixel_density()`, not `dpi_scale`.

## Pass 14

- Linux FetchContent path is unchanged by the keg-order fix: `/opt/homebrew` and `/usr/local` prepends stay macOS-only; configure logs when `brew` is absent (or the sdl3 keg is missing).
- CHANGELOG records pass 13 as landed on `main`; README layout line no longer hardcodes a stale pass range.
- Include `<utility>` in `Texture.cpp` (`std::move` of the GPU-lifetime pointer).

## Pass 13

- Homebrew SDL3 discovery matches the docs: `brew --prefix sdl3` keg first, then `/opt/homebrew`, then `/usr/local` (a leftover Intel tree can no longer shadow Apple Silicon).
- Sandbox rpath includes the keg `lib/` when `brew` is on `PATH`.
- BUILDING / README: keg vs Cellar, Retina three-space gotchas after high-DPI mapping.
- Drop unused `<utility>` in `Renderer.cpp`; reword the stale pass-2 focus-gain comment.

## Passes 1–12

- **1** — Textures, orthographic camera, entity / `Transform` helpers; teachable API comments; release preset; macOS rpath.
- **2** — Input / time hardening; letterboxed 2D drawing; focus loss releases held keys.
- **3** — Optional F1 / backtick debug HUD; `Color::lerp`; `--smoke` requires the BMP copied next to the binary; `cmake --install` places that BMP.
- **4** — Teardown / `GpuLifetime` seatbelt; HUD control legend; `Vec2::length_squared`.
- **5** — Re-apply letterbox on resize / DPI change; Space resets the identity logical view; `Rect::expanded` / `inset`.
- **6** — Mouse mapped window→logical on high-DPI (`SDL_RenderCoordinatesFromWindow`); `Window` pixel size / density; `clamp` / `clamp01`.
- **7** — Wheel zoom uses `Rect::contains_inclusive` (far present edges zoom; letterbox bars do not); three-space docs; `Cooldown`.
- **8** — Cooldown leftovers (non-finite remaining is expired); `clamp01` consistency; umbrella `midas.hpp` exports.
- **9** — Public-header audit; NaN-safe `Vec2::normalized_or_zero`; checkerboard non-finite channel fallbacks.
- **10** — `draw_entity` skips non-finite dest; `Camera{}` is origin, not the identity view; `zoom_toward` ignores NaN/Inf; smoke locks those contracts.
- **11** — `Entity.hpp` includes `<cmath>` itself; `clamp01` Inf docs; smoke asserts Inf zoom → 1.
- **12** — README / BUILDING smoke lists mention Inf zoom (docs-only).
