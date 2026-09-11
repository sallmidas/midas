# Midas architecture

Midas is split into a static engine library and a sandbox application. SDL3 is an implementation detail of the engine; public headers under `engine/include/midas/` do not include SDL.

## Targets

| Target | Path | Role |
| --- | --- | --- |
| `midas` | `engine/` | Core library (`midas::midas`) |
| `midas_sandbox` | `apps/sandbox/` | Windowed demo that exercises the public API |

## Public API

```
Engine     Owns the SDL video subsystem and the subsystems below.
           Runs a capped 60 Hz tick loop and pumps OS events.
Window     OS window (size, title). Native SDL handles stay private.
Input      Keyboard + mouse (down / pressed this tick), wheel, quit (close box).
           Mouse position is in logical render coordinates.
Time       Fixed timestep: `Time::tick_hz == 60`, `delta_seconds() == 1/60`.
Renderer   Clear, fill rect, textured quad, present. Applies an orthographic camera.
Texture    GPU image from RGBA8 pixels (`create_texture`) or a BMP (`load_bmp`).
Camera     2D ortho view: `position` is the world point at the viewport center.
Entity     Lightweight transform + size + optional texture. Not an ECS.
```

`Types.hpp` defines `Color`, `Vec2`, and `Rect`. `midas.hpp` is an umbrella include.

Games talk only to `Engine` and the types it returns:

```cpp
midas::Engine engine({.title = "Midas", .width = 1280, .height = 720});
auto pixels = midas::make_checkerboard_rgba(64, 64, midas::Color::gold(),
                                           midas::Color::charcoal(), 8);
auto sprite = engine.renderer().create_texture(64, 64, pixels);

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

Destroy a `Texture` before the `Renderer` / `Engine` that created it.

## Frame loop

Each tick:

1. Snapshot previous keyboard and mouse-button state (`key_pressed` / `mouse_pressed` are edges). Reset wheel delta.
2. Pump SDL events into `Input` (mouse events converted to logical render coordinates).
3. Sample `SDL_GetMouseState` and convert window pixels → logical coordinates.
4. Stop if quit was requested.
5. Invoke the tick callback (update + render).
6. Advance `Time::tick_index()`.
7. Sleep the remainder of `1/60` s so the loop holds 60 Hz even without vsync (for example `SDL_VIDEODRIVER=dummy`).

The renderer uses SDL3 logical presentation (`SDL_LOGICAL_PRESENTATION_LETTERBOX`) so drawing stays in the configured window coordinates on Retina / Apple Silicon displays.

## Camera

`Camera::position` is the world-space point shown at the center of the viewport. `zoom` is a uniform scale (clamped to `[0.25, 8]`).

The renderer starts with `zoom == 1` and `position` at the viewport center, so world units match logical pixels until the game moves the camera. `fill_rect` and `draw_texture` take world rectangles; `Camera::project` maps them to the screen. `Camera::zoom_toward` scales around a screen point (mouse-wheel zoom).

The sandbox pans with WASD / arrows (or right-mouse drag) and zooms with Q/E or the wheel. Space resets the view.

## Textures

Two upload paths, both SDL-private:

1. **CPU pixels** — `make_checkerboard_rgba` (or any tightly packed RGBA8 buffer) + `Renderer::create_texture`.
2. **BMP file** — `Renderer::load_bmp`. SDL3 loads BMP without SDL_image. PNG can wait until that dependency is worth it.

Textures use nearest-neighbor sampling so pixel art stays sharp when the camera zooms.

## Dependencies

SDL3 is resolved by `cmake/MidasSDL3.cmake`:

1. **Homebrew / system** — `brew --prefix sdl3` and `/opt/homebrew` on macOS.
2. **FetchContent** — SDL3 **3.4.16** if no config package is found.

The engine links `SDL3::SDL3` and does not leak that include path into public headers.
