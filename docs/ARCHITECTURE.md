# Midas architecture

Midas is split into a static engine library, a sandbox tech gym, and a v0 room game. SDL3 is an implementation detail of the engine; public headers under `engine/include/midas/` do not include SDL.

## Targets

| Target | Path | Role |
| --- | --- | --- |
| `midas` | `engine/` | Core library (`midas::midas`) |
| `midas_sandbox` | `apps/sandbox/` | Tech gym: camera, sprites, atlas, HUD |
| `midas_room` | `apps/room/` | v0 game: one room, WASD, key, door, hazard |

`ctest` in the build tree runs `midas_sandbox --smoke` and `midas_room --smoke` with `SDL_VIDEODRIVER=dummy`. Game v0 scope is in [GAME_V0.md](GAME_V0.md).

## Public API

```
Engine     Owns the SDL video subsystem and the subsystems below.
           Runs an accumulator frame loop: fixed 60 Hz simulation
           (`on_update`, up to `Engine::max_catch_up` ticks per display
           frame) then one `on_present`. `executable_directory()` is
           SDL_GetBasePath (asset lookup).
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
Time       Fixed simulation timestep: `Time::tick_hz == 60`,
           `delta_seconds() == 1/60` (use this for motion, once per
           `on_update`). `elapsed_seconds()` is wall-clock.
           `frame_seconds()` / `frames_per_second()` are the last
           **display frame**'s wall time including the 60 Hz pace sleep —
           HUD, not gameplay.
           `Cooldown` counts remaining seconds until `ready()` (tick with
           `delta_seconds()`, not `frame_seconds()`).
Renderer   Clear, fill rect, textured quad (optional texture-space source rect
           for atlas cells), present. Applies an orthographic camera.
           Owns GPU textures; `create_texture` / `load_bmp` return `TextureId`.
           `logical_width` / `logical_height` are the letterboxed present size
           (`EngineConfig`, stable across OS resizes). `logical_size()` is the
           same pair as a `Vec2` for camera math.
           `fill_screen_rect` / `draw_debug_text` are HUD space (ignore camera).
           The sandbox HUD is optional (F1 / backtick); `--smoke` skips it.
           Debug text is best-effort (dummy drivers cannot abort the demo).
TextureId  Opaque index+generation handle to a GPU image the Renderer owns.
           Copy freely. Default `{0,0}` is invalid. Destroying Engine/Renderer
           invalidates ids; `draw_texture` skips a stale/invalid id (no crash).
Camera     2D ortho view: `position` is the world point at the viewport center.
           Uniform zoom, aspect-correct visible rect, clamped to [0.25, 8].
Entity     Lightweight transform + size + optional `TextureId`. Not an ECS.
           `source` is an optional atlas cell in texture pixels (empty = whole
           texture). `Transform::then` composes a child without parent pointers.
AABB       `aabb_overlap(Rect, Rect)` is half-open overlap (same as
           `Rect::overlaps`). `aabb_move` slides a body along solids (X then Y).
           The room game uses both for walls / key / door.
```

`Types.hpp` defines `Color`, `Vec2`, `Rect`, NaN-safe `clamp` / `clamp01`, and the AABB helpers `aabb_overlap` / `aabb_move`. `Color::lerp` mixes two 0–1 colors (`t` is `clamp01`'d). `Vec2::length_squared` is `x*x+y*y` for comparisons without `hypot` (`length()`). `Vec2::normalized_or_zero` is the unit vector, or `{0,0}` if the length is zero / non-finite. `Rect::overlaps` / `Rect::contains` are half-open 2D AABB helpers (shared edges do not overlap). `aabb_overlap(a, b)` is the same test as `a.overlaps(b)`. `aabb_move(body, delta, solids)` applies X then Y and rejects an axis that would overlap a solid (slide along walls). `Rect::contains_inclusive` is the closed test (`[x, x+w] × [y, y+h]`) used for letterbox present bounds. `Rect::expanded` / `Rect::inset` grow or shrink every edge (a large inset can become an empty rect; a NaN amount is a no-op). `Cooldown` (in `Time.hpp`) is remaining-seconds until `ready()`; tick it with `delta_seconds()`. Non-finite `remaining` is expired. `midas.hpp` is the umbrella include (`Cooldown`, `clamp`, `aabb_overlap`, `aabb_move`, `contains_inclusive`, `TextureId`, `make_checkerboard_rgba`, and the rest of the public API).

Games talk only to `Engine` and the types it returns:

```cpp
midas::Engine engine({.title = "Midas", .width = 1280, .height = 720});
midas::TextureId sprite = engine.renderer().load_bmp(
    (engine.executable_directory() / "assets" / "midas_sprite.bmp").string());

midas::Entity tile;
tile.transform.position = {400.0f, 280.0f};
tile.size = {160.0f, 160.0f};
tile.texture = sprite;

midas::Camera camera = engine.renderer().camera();

engine.run(
    [&](midas::Engine& e) {
        if (e.input().key_pressed(midas::Key::Escape)) {
            e.request_quit();
            return;
        }
        camera.position.x += 40.0f * static_cast<float>(e.time().delta_seconds());
    },
    [&](midas::Engine& e) {
        e.renderer().set_camera(camera);
        e.renderer().clear(midas::Color::charcoal());
        midas::draw_entity(e.renderer(), tile);
        e.renderer().present();
    });
```

A sprite-sheet cell is the same textured quad with a source rect in texture pixels. One GPU upload, many cells — no re-upload per draw:

```cpp
e.renderer().draw_texture(atlas, dest, midas::Rect{0.0f, 0.0f, 32.0f, 32.0f});
e.renderer().draw_texture(atlas, dest2, midas::Rect{32.0f, 0.0f, 32.0f, 32.0f});
```

`Entity::source` is the same rectangle; leave it empty to sample the whole texture. `draw_entity` forwards a positive source to that overload.

`EngineConfig::max_ticks` stops the loop after N **simulation** ticks (`on_update`), used by `--smoke` / `MIDAS_SMOKE_FRAMES`. The env var name is historical: it counts sim ticks, not display presents. Closing the window sets `Input::quit_requested()` and ends the loop.

## Locked policies

- **Construction may throw.** `Engine`, `Window`, `Renderer`, `create_texture`, and `load_bmp` fail by exception. That is the setup path.
- **In-tick load failures must not unwind the loop.** `on_update` / `on_present` must not throw. If a game loads an asset during a tick, skip the draw or return an error; do not throw through `Engine::run()`. (The sandbox loads the BMP and atlas **before** `run()`.)
- **`SDL_Renderer` is the host** until a real game demands otherwise. No custom GPU backend, no `SDL_GPU`, no raw OpenGL in this tree.

## Shutdown

C++ destroys members in reverse declaration order. `Engine::Impl` is declared `SDL → Window → Renderer`, so teardown is:

1. `Renderer` — destroys the GPU textures in its slot vector, then `SDL_DestroyRenderer`.
2. `Window` — `SDL_DestroyWindow`.
3. `SDL_Quit`.

Games hold `TextureId` copies, not GPU resources. Destroying the `Engine` / `Renderer` invalidates every id it issued. `draw_texture` / `draw_entity` skip an invalid or stale id (no crash). Ids are not valid on a different renderer.

`Renderer::Impl` / `Window::Impl` own the native pointers so a constructor that throws after `SDL_CreateRenderer` / `SDL_CreateWindow` still destroys them. Those `Impl` types are not copyable or movable (a default move would duplicate the SDL pointer).

## Frame loop

Simulation is fixed at 60 Hz (`Time::tick_hz`). Display presents are decoupled: one present per display frame after 0..`Engine::max_catch_up` (4) simulation ticks.

Each **display frame**:

1. Measure wall time since the previous frame. Clamp that elapsed time to at most `max_catch_up` ticks (a debugger pause cannot queue unbounded catch-up). Add it to an accumulator on `Engine` (not on `Time`).
2. Snapshot previous keyboard and mouse-button state (`key_pressed` / `mouse_pressed` are edges). Reset wheel delta.
3. Pump SDL events into `Input`. Resize, pixel-size, and display-scale changes re-apply letterbox presentation and zero mouse delta. Mouse *event* coordinates are converted to logical space; sampled mouse position is applied in step 4.
4. Sample `SDL_GetMouseState` (**window coordinates**, not framebuffer pixels) and convert through `SDL_RenderCoordinatesFromWindow` (pixel density + letterbox → logical). A failed conversion keeps the last logical sample — it does not fall back to window or pixel coords, which would break `Camera::zoom_toward` on a high-DPI or letterboxed window.
5. Stop if quit was requested (close box). No `on_update` / `on_present` this frame.
6. While the accumulator holds at least `1/60` s and fewer than `max_catch_up` ticks have run this frame:
   - Extra catch-up ticks call `Input::begin_frame` again **without** re-pumping OS events, so `key_pressed`, wheel, and `mouse_delta` fire once per display frame. Held keys (`key_down`) still apply on every simulation tick (WASD pan stays at 60 Hz).
   - Invoke `on_update` (movement / gameplay only). `Time::delta_seconds()` is always `1/60`.
   - Advance `Time::tick_index()` and subtract `1/60` s from the accumulator. Leftover time stays for the next frame.
   - Stop the loop if `request_quit` (Esc) or `EngineConfig::max_ticks` simulation ticks have completed.
7. Invoke `on_present` once (clear / draw / `present`). Skipped on Esc / close-box so the last frame is not a half-updated draw.
8. If this display frame used less than `1/60` s of wall time, sleep the remainder so presents hold 60 Hz even without vsync (for example `SDL_VIDEODRIVER=dummy`). A catch-up frame that already overran does not sleep.
9. Record that **display frame**'s wall time on `Time` (`frame_seconds` / `frames_per_second`), including the sleep — or the overrun if the frame ran long.

Gameplay must use `delta_seconds()` (fixed `1/60`), not `frame_seconds()`. A hitch then does not fling the camera; the engine runs extra simulation ticks (up to 4) and presents once. The HUD can still show the dip in present rate.

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
- **HUD:** `fill_screen_rect` and `draw_debug_text` skip the camera so an overlay does not pan or zoom with the world. The sandbox draws FPS, camera position, zoom, and a one-line WASD/zoom/Space/F1 legend this way; **F1** or **`** toggles it. `--smoke` leaves it off so CI does not depend on debug text. Debug text failure is ignored (dummy drivers). The HUD banner uses `Rect::expanded` for padding. Interactive mode logs `logical / window / pixels / density` once at startup; `--smoke` does not.

## Camera

`Camera::position` is the world-space point shown at the center of the viewport. `zoom` is a **uniform** scale, clamped to `[0.25, 8]`.

Visible world size is `(logical_w / zoom)` by `(logical_h / zoom)`, so the view aspect matches the logical present size (aspect-correct ortho). Circles stay circles; letterboxing handles a window whose pixel aspect differs.

The renderer starts with `zoom == 1` and `position` at the viewport center, so world units match logical present pixels until the game moves the camera. A default `Camera{}` is zoom 1 at world origin — copy `Renderer::camera()` for that identity view. `fill_rect` and `draw_texture` take world dest rectangles; `Camera::project` maps them to the screen. `draw_texture` may also take a **source** `Rect` in texture pixel space (sprite-sheet cell) — one GPU upload, many cells. Omit `src` (the three-argument overload, or `Entity::source` left empty) to sample the whole texture. A non-finite or non-positive source is skipped, same as dest. `Camera::zoom_toward` scales around a **logical** screen point (`Input::mouse_position()`, not window or framebuffer pixels) and ignores non-finite or non-positive multipliers. Projection uses `clamped_zoom()` so a zero/NaN zoom cannot divide by zero.

The sandbox pans with WASD / arrows (or right-mouse drag) at constant **screen-space** speed (`pan / zoom`) and zooms with Q/E or the wheel (wheel zoom is skipped while the cursor is **outside** the closed logical present rect — letterbox bars, not the far edge of the view). **Space** resets to the identity view for the current logical viewport (center, zoom 1) and sanitizes pan/zoom so a NaN cannot stick; the same tick does not also apply WASD/wheel/drag. **Esc** requests quit and returns before `present`. Held WASD and the right mouse button are released if the window loses focus, so the camera cannot keep sliding while you are in another app. Coming back into focus re-reads the OS keyboard and mouse buttons, so a still-held W resumes pan without needing a new key-down — and without synthesizing a `key_pressed` edge (Escape would quit, F1 would toggle the HUD). Key/button events while unfocused are ignored. Mouse-wheel and right-drag deltas are clamped so a cursor warp or a wild trackpad burst cannot jump the view. A HUD in the top-left shows the fixed `dt`, wall-clock FPS, camera, and a one-line control legend (toggle with F1 or backtick).

## Textures

Two upload paths, both SDL-private. Both return a `TextureId`; the renderer owns the GPU image:

1. **CPU pixels** — `make_checkerboard_rgba` (or any tightly packed RGBA8 buffer) + `Renderer::create_texture`.
2. **BMP file** — `Renderer::load_bmp`. SDL3 loads BMP without SDL_image. PNG can wait until that dependency is worth it.

Both paths throw on bad input (empty path, missing file, undersized pixel buffer, invalid size). `load_bmp` puts the path in the exception; it does not return a dummy texture. `make_checkerboard_rgba` `clamp01`s finite channels; non-finite RGB becomes 0 and non-finite alpha becomes 1 (the same fallback as renderer color mods).

Query size with `texture_width` / `texture_height` (0 if the id is invalid or stale). `texture_valid` is false for the default id and for a generation the registry does not have.

`Renderer::draw_texture(id, dest, src, tint)` samples `src` in texture pixels (top-left origin). A missing or stale `TextureId` is a no-op (no crash). The sandbox uploads a 64×32 two-cell sheet once (`make_checkerboard_rgba` with `cell_size == 32`) and draws the gold cell and the bronze cell side by side — same handle, two source rects, no re-upload. The three-argument `draw_texture` (and an `Entity` with default `source`) still draws the whole image.

Textures use nearest-neighbor sampling (`SDL_SCALEMODE_NEAREST`) so pixel art stays sharp when the camera zooms. Linear filtering is intentionally not exposed yet.

The sandbox locates `assets/midas_sprite.bmp` via `Engine::executable_directory()`, `argv[0]`, `./assets`, and the source tree `apps/sandbox/assets`. `--smoke` only accepts a BMP next to the binary (the CMake copy / install rule) and fails if that file is missing. An interactive run logs the search and falls back to a generated checkerboard. Separately, the sandbox generates a 64×32 two-cell atlas once (`create_texture`) and draws both cells with source rects.

`midas_room` loads `assets/room_atlas.bmp` the same way (CMake copies `apps/room/assets/` next to the binary). It draws Jim's 6-cell sheet with `draw_texture(..., src)`. If the file is missing or `load_bmp` fails, the room falls back to solid rects — including during `--smoke`.

## Entities and AABB

`Transform` is position + scale (no rotation). `parent.then(local)` composes a child in the parent's space — a scene-graph starter without storing parent pointers that can dangle.

`Entity` is a drawable bag of data: transform, unscaled size, color (fill or texture tint), an optional `TextureId` (default invalid = solid fill), and an optional texture-space `source` rect (atlas cell; empty / non-positive size means the whole texture). Keep entities in an array; draw with `draw_entity` (skips a non-finite or non-positive dest). A minted-looking but stale handle is skipped by `draw_texture`, not turned into a fill.

`Rect` is the 2D AABB (`x, y, w, h` with top-left origin). `contains` is half-open (`[x, x+w) × [y, y+h)`). `overlaps` / `aabb_overlap` use the same edges, so rectangles that only share a boundary do not overlap, and a zero-size rect is empty. `aabb_move` tries X then Y against a span of solids so a body can slide along a wall. `contains_inclusive` is closed (`[x, x+w] × [y, y+h]`) for letterbox present bounds. `expanded(amount)` / `inset(amount)` grow or shrink every edge; a large inset can yield a non-positive size (empty). `Entity::overlaps` calls `aabb_overlap` on the two bounds. Width/height should stay non-negative.

The v0 room app (`midas_room`) is the first game on this API: walls are solids, the locked door is a solid, key pickup is overlap, overlapping the open door wins, and overlapping a stationary hazard fail AABB restarts via **R**. See [GAME_V0.md](GAME_V0.md).

`Color::lerp(a, b, t)` is the 0–1 mix used by the sandbox plinth (gold toward bronze). `t` is `clamp01`'d (NaN / negative → 0; Inf / above 1 → 1). `clamp` / `clamp01` are the NaN-safe float helpers (`std::clamp` is undefined when `lo > hi`). `Vec2::normalized_or_zero` is the unit vector used for WASD pan (zero / NaN / Inf → `{0,0}`). `Cooldown` is remaining-seconds until `ready()`; tick it with the fixed `delta_seconds()` step. Non-finite `remaining` is expired (`ready()`, and `tick` snaps it to 0).

## Dependencies

SDL3 is resolved by `cmake/MidasSDL3.cmake`:

1. **Homebrew / system** — `brew --prefix sdl3` (the keg, e.g. `/opt/homebrew/opt/sdl3`), then `/opt/homebrew`, then `/usr/local` on **macOS** so a leftover Intel tree cannot shadow Apple Silicon. Those two roots are not prepended on Linux (`/usr/local` is a common empty prefix there). `find_package(SDL3 3.2)` uses CMake’s system prefixes (`libsdl3-dev`) and Linux Homebrew if `brew` is on `PATH`. 3.2 is the floor (`SDL_RenderDebugText` for the HUD).
2. **FetchContent** — SDL3 **3.4.16** if no 3.2+ config package is found.

The engine links `SDL3::SDL3` and does not leak that include path into public headers.
