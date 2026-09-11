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
           `width` / `height` are live **window coordinates** (resizable).
           `pixel_width` / `pixel_height` are the drawable; `pixel_density()`
           is pixels per window coordinate (often 2 on Retina). After a resize
           or DPI change these diverge from the logical present size — camera
           and mouse still use `Renderer::logical_*`. Do not scale mouse by
           `pixel_density()`.
Input      Keyboard + mouse (down / pressed this tick), wheel, quit (close box).
           Mouse position is in logical render coordinates (not window coords
           and not framebuffer pixels).
           `mouse_delta()` is zero until the first real sample, on focus gain,
           on resize / DPI change, and while unfocused. Mouse/wheel deltas are
           clamped (no warp jumps).
           Focus loss releases held keys and buttons (`window_focused()`).
           Focus gain re-reads OS key/button state so held WASD still pans
           without synthesizing key_pressed (Escape / F1 / Space).
           Unfocused key/button events are ignored. A resize or display-scale
           change zeros mouse_delta so letterbox remapping cannot jump a
           right-drag pan.
Time       Fixed timestep: `Time::tick_hz == 60`, `delta_seconds() == 1/60`
           (use this for motion). `elapsed_seconds()` is wall-clock.
           `frame_seconds()` / `frames_per_second()` are the last tick's wall
           time including the 60 Hz sleep — HUD, not gameplay.
           `Cooldown` counts remaining seconds until `ready()` (tick with
           `delta_seconds()`, not `frame_seconds()`).
Renderer   Clear, fill rect, textured quad, present. Applies an orthographic camera.
           `logical_width` / `logical_height` are the letterboxed present size
           (`EngineConfig`, stable across OS resizes). `logical_size()` is the
           same pair as a `Vec2` for camera math.
           `fill_screen_rect` / `draw_debug_text` are HUD space (ignore camera).
           The sandbox HUD is optional (F1 / backtick); `--smoke` skips it.
           Debug text is best-effort (dummy drivers cannot abort the demo).
Texture    GPU image from RGBA8 pixels (`create_texture`) or a BMP (`load_bmp`).
           Nearest-neighbor sampling; destroy before the Renderer.
           A texture that outlives the renderer skips SDL_DestroyTexture.
Camera     2D ortho view: `position` is the world point at the viewport center.
           Uniform zoom, aspect-correct visible rect, clamped to [0.25, 8].
Entity     Lightweight transform + size + optional texture. Not an ECS.
           `Transform::then` composes a child without parent pointers.
```

`Types.hpp` defines `Color`, `Vec2`, `Rect`, and NaN-safe `clamp` / `clamp01`. `Color::lerp` mixes two 0–1 colors (`t` is `clamp01`'d). `Vec2::length_squared` is `x*x+y*y` for comparisons without `hypot` (`length()`). `Vec2::normalized_or_zero` is the unit vector, or `{0,0}` if the length is zero / non-finite. `Rect::overlaps` / `Rect::contains` are half-open 2D AABB helpers (shared edges do not overlap). `Rect::contains_inclusive` is the closed test (`[x, x+w] × [y, y+h]`) used for letterbox present bounds. `Rect::expanded` / `Rect::inset` grow or shrink every edge (a large inset can become an empty rect; a NaN amount is a no-op). `Cooldown` (in `Time.hpp`) is remaining-seconds until `ready()`; tick it with `delta_seconds()`. Non-finite `remaining` is expired. `midas.hpp` is the umbrella include (`Cooldown`, `clamp`, `contains_inclusive`, `make_checkerboard_rgba`, and the rest of the public API).

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

## Shutdown

C++ destroys members in reverse declaration order. `Engine::Impl` is declared `SDL → Window → Renderer`, so teardown is:

1. Game `Texture`s — the sandbox declares the BMP *after* `Engine`, so it dies first (the SDL order to learn).
2. `Renderer` — sets a shared `GpuLifetime` flag, then `SDL_DestroyRenderer` (SDL also frees leftover GPU textures).
3. `Window` — `SDL_DestroyWindow`.
4. `SDL_Quit`.

If step 1 is skipped, step 2 still tears the GPU down safely: a late `Texture` destructor sees `!alive` and does not call `SDL_DestroyTexture` on a freed handle. Each texture holds a typed `shared_ptr` to that flag (the public constructor takes `shared_ptr<void>` so the header does not name the internal type). `Renderer::Impl` / `Window::Impl` own the native pointers so a constructor that throws after `SDL_CreateRenderer` / `SDL_CreateWindow` still destroys them. Those `Impl` types are not copyable or movable (a default move would duplicate the SDL pointer).

Destroy a `Texture` before the `Renderer` / `Engine` that created it. `Texture` is move-only; move-assignment releases the previous GPU texture (RAII in `Texture::Impl`).

## Frame loop

Each tick:

1. Snapshot previous keyboard and mouse-button state (`key_pressed` / `mouse_pressed` are edges). Reset wheel delta.
2. Pump SDL events into `Input`. Resize, pixel-size, and display-scale changes re-apply letterbox presentation and zero mouse delta. Mouse *event* coordinates are converted to logical space; sampled mouse position is applied in step 3.
3. Sample `SDL_GetMouseState` (**window coordinates**, not framebuffer pixels) and convert through `SDL_RenderCoordinatesFromWindow` (pixel density + letterbox → logical). A failed conversion keeps the last logical sample — it does not fall back to window or pixel coords, which would break `Camera::zoom_toward` on a high-DPI or letterboxed window.
4. Stop if quit was requested.
5. Invoke the tick callback (update + render).
6. Advance `Time::tick_index()`.
7. Sleep the remainder of `1/60` s so the loop holds 60 Hz even without vsync (for example `SDL_VIDEODRIVER=dummy`).
8. Record that tick's wall time on `Time` (`frame_seconds` / `frames_per_second`), including the sleep — or the overrun if the tick ran long.

Gameplay must use `delta_seconds()` (fixed `1/60`), not `frame_seconds()`. A hitch then does not fling the camera; the HUD can still show the dip in FPS.

The renderer uses SDL3 logical presentation (`SDL_LOGICAL_PRESENTATION_LETTERBOX`) so drawing stays in **logical present pixels** (`EngineConfig` width × height, 1280×720 in the sandbox). The OS window is **resizable** and created with `SDL_WINDOW_HIGH_PIXEL_DENSITY`. Letterboxing is re-applied on `WINDOW_RESIZED` / `PIXEL_SIZE_CHANGED` / `DISPLAY_SCALE_CHANGED` so a driver cannot drop the mapping.

### Three coordinate spaces

These names are used the same way in headers, the sandbox, and this document:

| Space | API | SDL | Role |
| --- | --- | --- | --- |
| **Logical present pixels** | `Renderer::logical_width/height` / `logical_size()` | `SDL_SetRenderLogicalPresentation` | Camera viewport, drawing, `Input` mouse. Captured from `EngineConfig`; does not follow OS resizes. |
| **Window coordinates** | `Window::width/height` | `SDL_GetWindowSize`, `SDL_GetMouseState` | Live OS client. Changes on resize. |
| **Framebuffer pixels** | `Window::pixel_width/height`, `pixel_density()` | `SDL_GetWindowSizeInPixels` | Drawable. Often 2× window size on Retina. |

Never pass window or pixel size to `Camera` / `zoom_toward`. Never multiply mouse coordinates by `pixel_density()` — the engine mapping already includes it.

- **Matching aspect (typical Retina / Apple Silicon):** the drawable has more pixels (`pixel_density()` often 2), but the aspect matches, so there are no bars — each logical pixel covers several drawable pixels. Window center maps to logical center; framebuffer center is the logical *corner*.
- **Mismatched aspect (resized window, odd display):** black bars pad the extra drawable region. The logical 1280×720 rectangle is undistorted in the middle.
- **Present rect:** SDL maps drawable content onto the **closed** logical rect `[0, logical_w] × [0, logical_h]`. Far edges are still the view. Letterbox bars yield a point outside that closed rect. Half-open `Rect::contains` (AABB) rejects the far edge — wheel zoom uses `contains_inclusive`.
- **Mouse:** `Engine` samples `SDL_GetMouseState` (window coordinates) and converts with `SDL_RenderCoordinatesFromWindow` (pixel density + letterbox → logical). Event coordinates are converted the same way (`SDL_ConvertEventToRenderCoordinates`) but mouse position comes from the sampled state. A failed conversion keeps the last logical sample — it does not fall back to window or pixel coords. The SDL converter needs a renderer; `--smoke` checks the three-space policy (and the cost of mixing spaces) without calling it. A resize or DPI change zeros `mouse_delta` so the remapping cannot jump a right-drag pan.
- **Camera:** always pass `Renderer::logical_width/height` as the viewport, never `Window::width/height` and never `pixel_width/height`. `zoom_toward` takes the same logical space as `Input::mouse_position()`. World fills and textures share `Camera::project`, so a gold square and a gold-tinted sprite of the same world rect stay aligned at any zoom. Tint is a per-texel multiply, not a function of dest size.
- **HUD:** `fill_screen_rect` and `draw_debug_text` skip the camera so an overlay does not pan or zoom with the world. The sandbox draws FPS, camera position, zoom, and a one-line WASD/zoom/Space/F1 legend this way; **F1** or **`** toggles it. `--smoke` leaves it off so CI does not depend on debug text. Debug text failure is ignored (dummy drivers). The HUD banner uses `Rect::expanded` for padding. Interactive mode logs logical vs window vs pixel size once at startup; `--smoke` does not.

## Camera

`Camera::position` is the world-space point shown at the center of the viewport. `zoom` is a **uniform** scale, clamped to `[0.25, 8]`.

Visible world size is `(logical_w / zoom)` by `(logical_h / zoom)`, so the view aspect matches the logical present size (aspect-correct ortho). Circles stay circles; letterboxing handles a window whose pixel aspect differs.

The renderer starts with `zoom == 1` and `position` at the viewport center, so world units match logical present pixels until the game moves the camera. A default `Camera{}` is zoom 1 at world origin — copy `Renderer::camera()` for that identity view. `fill_rect` and `draw_texture` take world rectangles; `Camera::project` maps them to the screen. `Camera::zoom_toward` scales around a **logical** screen point (`Input::mouse_position()`, not window or framebuffer pixels) and ignores non-finite or non-positive multipliers. Projection uses `clamped_zoom()` so a zero/NaN zoom cannot divide by zero.

The sandbox pans with WASD / arrows (or right-mouse drag) at constant **screen-space** speed (`pan / zoom`) and zooms with Q/E or the wheel (wheel zoom is skipped while the cursor is **outside** the closed logical present rect — letterbox bars, not the far edge of the view). **Space** resets to the identity view for the current logical viewport (center, zoom 1) and sanitizes pan/zoom so a NaN cannot stick; the same tick does not also apply WASD/wheel/drag. **Esc** requests quit and returns before `present`. Held WASD and the right mouse button are released if the window loses focus, so the camera cannot keep sliding while you are in another app. Coming back into focus re-reads the OS keyboard and mouse buttons, so a still-held W resumes pan without needing a new key-down — and without synthesizing a `key_pressed` edge (Escape would quit, F1 would toggle the HUD). Key/button events while unfocused are ignored. Mouse-wheel and right-drag deltas are clamped so a cursor warp or a wild trackpad burst cannot jump the view. A HUD in the top-left shows the fixed `dt`, wall-clock FPS, camera, and a one-line control legend (toggle with F1 or backtick).

## Textures

Two upload paths, both SDL-private:

1. **CPU pixels** — `make_checkerboard_rgba` (or any tightly packed RGBA8 buffer) + `Renderer::create_texture`.
2. **BMP file** — `Renderer::load_bmp`. SDL3 loads BMP without SDL_image. PNG can wait until that dependency is worth it.

Both paths throw on bad input (empty path, missing file, undersized pixel buffer, invalid size). `load_bmp` puts the path in the exception; it does not return a dummy texture. `make_checkerboard_rgba` `clamp01`s finite channels; non-finite RGB becomes 0 and non-finite alpha becomes 1 (the same fallback as renderer color mods).

Textures use nearest-neighbor sampling (`SDL_SCALEMODE_NEAREST`) so pixel art stays sharp when the camera zooms. Linear filtering is intentionally not exposed yet.

The sandbox locates `assets/midas_sprite.bmp` via `Engine::executable_directory()`, `argv[0]`, `./assets`, and the source tree `apps/sandbox/assets`. `--smoke` only accepts a BMP next to the binary (the CMake copy / install rule) and fails if that file is missing. An interactive run logs the search and falls back to a generated checkerboard.

## Entities and AABB

`Transform` is position + scale (no rotation). `parent.then(local)` composes a child in the parent's space — a scene-graph starter without storing parent pointers that can dangle.

`Entity` is a drawable bag of data: transform, unscaled size, color (fill or texture tint), and an optional **non-owning** `const Texture*`. Keep entities in an array; draw with `draw_entity` (skips a non-finite or non-positive dest).

`Rect` is the 2D AABB (`x, y, w, h` with top-left origin). `contains` is half-open (`[x, x+w) × [y, y+h)`). `overlaps` uses the same edges, so rectangles that only share a boundary do not overlap, and a zero-size rect is empty. `contains_inclusive` is closed (`[x, x+w] × [y, y+h]`) for letterbox present bounds. `expanded(amount)` / `inset(amount)` grow or shrink every edge; a large inset can yield a non-positive size (empty). `Entity::overlaps` is the collision starter. Width/height should stay non-negative.

`Color::lerp(a, b, t)` is the 0–1 mix used by the sandbox plinth (gold toward bronze). `t` is `clamp01`'d (NaN / negative → 0; Inf / above 1 → 1). `clamp` / `clamp01` are the NaN-safe float helpers (`std::clamp` is undefined when `lo > hi`). `Vec2::normalized_or_zero` is the unit vector used for WASD pan (zero / NaN / Inf → `{0,0}`). `Cooldown` is remaining-seconds until `ready()`; tick it with the fixed `delta_seconds()` step. Non-finite `remaining` is expired (`ready()`, and `tick` snaps it to 0).

## Dependencies

SDL3 is resolved by `cmake/MidasSDL3.cmake`:

1. **Homebrew / system** — `brew --prefix sdl3` (the keg, e.g. `/opt/homebrew/opt/sdl3`), then `/opt/homebrew`, then `/usr/local` on macOS so a leftover Intel tree cannot shadow Apple Silicon; `find_package(SDL3 3.2)` on Linux (and Linux Homebrew if `brew` is on `PATH`). 3.2 is the floor (`SDL_RenderDebugText` for the HUD).
2. **FetchContent** — SDL3 **3.4.16** if no 3.2+ config package is found.

The engine links `SDL3::SDL3` and does not leak that include path into public headers.
