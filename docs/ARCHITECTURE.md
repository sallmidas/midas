# Midas architecture

Midas is split into a static engine library and a sandbox application. SDL3 is an implementation detail of the engine; public headers under `engine/include/midas/` do not include SDL.

## Targets

| Target | Path | Role |
| --- | --- | --- |
| `midas` | `engine/` | Core library (`midas::midas`) |
| `midas_sandbox` | `apps/sandbox/` | Windowed demo that exercises the public API |

`ctest` in the build tree runs `midas_sandbox --smoke` with `SDL_VIDEODRIVER=dummy`.

## Public API

```
Engine     Owns the SDL video subsystem and the subsystems below.
           Runs a capped 60 Hz tick loop and pumps OS events.
           `executable_directory()` is SDL_GetBasePath (asset lookup).
Window     OS window (size, title). Native SDL handles stay private.
Input      Keyboard + mouse (down / pressed this tick), wheel, quit (close box).
           Mouse position is in logical render coordinates.
           `mouse_delta()` is zero until the first real sample (no first-frame jump).
Time       Fixed timestep: `Time::tick_hz == 60`, `delta_seconds() == 1/60`.
           `elapsed_seconds()` is wall-clock time, not a variable dt.
Renderer   Clear, fill rect, textured quad, present. Applies an orthographic camera.
           `logical_width` / `logical_height` are the camera viewport.
Texture    GPU image from RGBA8 pixels (`create_texture`) or a BMP (`load_bmp`).
           Nearest-neighbor sampling; destroy before the Renderer.
Camera     2D ortho view: `position` is the world point at the viewport center.
           Uniform zoom, aspect-correct visible rect, clamped to [0.25, 8].
Entity     Lightweight transform + size + optional texture. Not an ECS.
           `Transform::then` composes a child without parent pointers.
```

`Types.hpp` defines `Color`, `Vec2`, and `Rect`. `Rect::overlaps` / `Rect::contains` are the 2D AABB helpers. `midas.hpp` is an umbrella include.

Games talk only to `Engine` and the types it returns:

```cpp
midas::Engine engine({.title = "Midas", .width = 1280, .height = 720});
auto sprite = engine.renderer().load_bmp(
    (engine.executable_directory() / "assets" / "midas_sprite.bmp").string());

midas::Entity tile;
tile.transform.position = {400.0f, 280.0f};
tile.size = {160.0f, 160.0f};
tile.texture = &sprite;

midas::Camera camera = engine.renderer().camera();

engine.run([&](midas::Engine& e) {
    if (e.input().key_pressed(midas::Key::Escape)) {
        e.request_quit();
        return;
    }
    e.renderer().set_camera(camera);
    e.renderer().clear(midas::Color::charcoal());
    midas::draw_entity(e.renderer(), tile);
    e.renderer().present();
});
```

`EngineConfig::max_ticks` stops the loop after N ticks (used by `--smoke` / `MIDAS_SMOKE_FRAMES`). Closing the window sets `Input::quit_requested()` and ends the loop.

Destroy a `Texture` before the `Renderer` / `Engine` that created it. `Texture` is move-only; move-assignment releases the previous GPU texture (RAII in `Texture::Impl`).

## Frame loop

Each tick:

1. Snapshot previous keyboard and mouse-button state (`key_pressed` / `mouse_pressed` are edges). Reset wheel delta.
2. Pump SDL events into `Input` (mouse events converted to logical render coordinates).
3. Sample `SDL_GetMouseState` and convert window pixels → logical coordinates.
4. Stop if quit was requested.
5. Invoke the tick callback (update + render).
6. Advance `Time::tick_index()`.
7. Sleep the remainder of `1/60` s so the loop holds 60 Hz even without vsync (for example `SDL_VIDEODRIVER=dummy`).

The renderer uses SDL3 logical presentation (`SDL_LOGICAL_PRESENTATION_LETTERBOX`) so drawing stays in the configured window coordinates on Retina / Apple Silicon displays and on a resized Linux window. Camera math should use `Renderer::logical_width/height`, not raw drawable pixels.

## Camera

`Camera::position` is the world-space point shown at the center of the viewport. `zoom` is a **uniform** scale, clamped to `[0.25, 8]`.

Visible world size is `(logical_w / zoom)` by `(logical_h / zoom)`, so the view aspect matches the logical window (aspect-correct ortho). Circles stay circles; letterboxing handles a window whose pixel aspect differs.

The renderer starts with `zoom == 1` and `position` at the viewport center, so world units match logical pixels until the game moves the camera. `fill_rect` and `draw_texture` take world rectangles; `Camera::project` maps them to the screen. `Camera::zoom_toward` scales around a screen point (mouse-wheel zoom) and ignores non-positive multipliers. Projection uses `clamped_zoom()` so a zero/NaN zoom cannot divide by zero.

The sandbox pans with WASD / arrows (or right-mouse drag) at constant **screen-space** speed (`pan / zoom`) and zooms with Q/E or the wheel. Space resets the view.

## Textures

Two upload paths, both SDL-private:

1. **CPU pixels** — `make_checkerboard_rgba` (or any tightly packed RGBA8 buffer) + `Renderer::create_texture`.
2. **BMP file** — `Renderer::load_bmp`. SDL3 loads BMP without SDL_image. PNG can wait until that dependency is worth it.

Both paths throw on bad input (empty path, missing file, undersized pixel buffer, invalid size). `load_bmp` puts the path in the exception; it does not return a dummy texture.

Textures use nearest-neighbor sampling (`SDL_SCALEMODE_NEAREST`) so pixel art stays sharp when the camera zooms. Linear filtering is intentionally not exposed yet.

The sandbox locates `assets/midas_sprite.bmp` via `Engine::executable_directory()`, `argv[0]`, `./assets`, and the source tree `apps/sandbox/assets`. `--smoke` fails if that BMP is missing. An interactive run logs the search and falls back to a generated checkerboard.

## Entities and AABB

`Transform` is position + scale (no rotation). `parent.then(local)` composes a child in the parent's space — a scene-graph starter without storing parent pointers that can dangle.

`Entity` is a drawable bag of data: transform, unscaled size, color (fill or texture tint), and an optional **non-owning** `const Texture*`. Keep entities in an array; draw with `draw_entity`.

`Rect` is the 2D AABB (`x, y, w, h` with top-left origin). `Rect::overlaps`, `Rect::contains`, and `Entity::overlaps` are the collision starter. Width/height should stay non-negative.

## Dependencies

SDL3 is resolved by `cmake/MidasSDL3.cmake`:

1. **Homebrew / system** — `brew --prefix sdl3` and `/opt/homebrew` on macOS; `find_package` on Linux.
2. **FetchContent** — SDL3 **3.4.16** if no config package is found.

The engine links `SDL3::SDL3` and does not leak that include path into public headers.
