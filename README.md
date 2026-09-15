# Midas

Midas is a C++20 game engine. The public API covers a window, input, a fixed 60 Hz simulation loop with a decoupled present, a 2D renderer with textures (including sprite-sheet source rects) and an orthographic camera, AABB helpers (`aabb_overlap` / `aabb_move`), and lightweight entity/transform helpers. The sandbox is the tech gym: a gold-and-bronze scene you can pan and zoom. `midas_room` is the v0 game: one room, WASD, walls, a key, a door, and one stationary hazard.

## Layout

```
engine/include/midas/   Public headers (`midas.hpp` umbrella: Engine, Window,
                        Input, Time/`Cooldown`, Renderer, Texture/`TextureId`,
                        Camera, Entity, Types/`aabb_overlap`). SDL stays out
                        of these files.
engine/src/             Engine implementation (SDL3 kept private)
apps/sandbox/           Tech gym: camera, sprites, entities, Esc/quit
apps/sandbox/assets/    Demo BMP copied next to the sandbox binary
apps/room/              v0 game: one room, WASD, key, door, hazard, restart
apps/room/assets/       Jim's 192×32 room atlas BMP (copied next to midas_room)
docs/                   Architecture, game v0 notes, macOS / Linux build
CHANGELOG.md            Fine-tune pass highlights
cmake/                  Homebrew / system SDL3 discovery, FetchContent fallback
```

## Quick start

**macOS (Homebrew on Apple Silicon or Intel):**

```bash
brew install cmake ninja sdl3
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/midas_sandbox
./build/debug/bin/midas_room
```

CMake finds the keg via `brew --prefix sdl3` (`/opt/homebrew/opt/sdl3` on Apple Silicon, `/usr/local/opt/sdl3` on Intel) — not a `Cellar/sdl3/<version>` path. Interactive startup prints `logical / window / pixels / density` so Retina (often 2× framebuffer, same aspect, no letterbox bars) is obvious. Do not scale mouse by that density.

**Linux:**

```bash
sudo apt install cmake ninja-build g++ pkg-config   # or clang
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/midas_sandbox
./build/debug/bin/midas_room
```

If `c++` on `PATH` is Clang but CMake cannot link `libstdc++`, configure with `CC=gcc CXX=g++ cmake --preset debug`.

A `release` preset is the same commands with `release` instead of `debug`.

If SDL3 is not installed, CMake fetches **SDL3 3.4.16** automatically (needs network the first time).

### Sandbox controls

| Input | Action |
| --- | --- |
| **Esc** or close the window | Quit |
| **WASD** / arrow keys | Pan (constant screen-space speed) |
| Right mouse drag | Pan with the cursor |
| **Q** / **E** | Zoom out / in around the view center |
| Mouse wheel | Zoom toward the cursor (skipped in letterbox bars, not at the far edge of the view) |
| **Space** | Reset pan and zoom to the identity logical view |
| **F1** or **`** | Toggle the screen-space debug HUD (off during `--smoke`) |

Zoom is uniform and clamped to **0.25–8**. The view is aspect-correct: the visible world matches the 1280×720 **logical present** size. The OS window is resizable and high-DPI; letterboxing keeps that logical view undistorted. Camera and mouse use `Renderer::logical_size()` (logical present pixels), not live `Window::width/height` (window coordinates) and not `Window::pixel_width/height` (framebuffer). Interactive mode logs `logical / window / pixels / density` once at startup. Wheel zoom uses the closed present rect (`Rect::contains_inclusive`) so the far edges of the view still zoom — only letterbox bars are skipped.

### Room controls (`midas_room`)

v0 is one dungeon room. The sandbox stays the camera/atlas gym; this binary is the playable slice. See [docs/GAME_V0.md](docs/GAME_V0.md).

| Input | Action |
| --- | --- |
| **WASD** / arrows | Move the player (AABB vs walls) |
| Overlap the key | Pick up; door unlocks (gold) |
| Overlap the open door | Win |
| Overlap the red pit | Fail (freeze + overlay; **R** restarts) |
| **R** | Restart the room (also after a win or fail) |
| **Esc** or close the window | Quit |

Fixed room camera (identity logical view). Drawing is Jim's 192×32 BMP atlas (`room_atlas.bmp`, 32px cells: player / wall / key / door shut / door open / pit) via `TextureId` + source rects. Solid rects if that BMP is missing or load fails.

Headless smoke (a few 60 Hz **simulation** ticks, then exit):

```bash
SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_sandbox --smoke
SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_room --smoke
# or: ctest --preset debug
```

Sandbox `--smoke` runs a header-only math self-check **before** SDL (clamp, Vec2 including `normalized_or_zero`, Color, Rect AABB / `aabb_overlap` / `aabb_move` / `expanded` / `contains_inclusive`, Camera including Inf zoom → 1, Transform, Entity, `TextureId`, Cooldown, CPU `make_checkerboard_rgba`, three-space mouse policy), then a few dummy-video **simulation** ticks (`EngineConfig::max_ticks` / `MIDAS_SMOKE_FRAMES` count `on_update` calls, not presents) that **require** `assets/midas_sprite.bmp` next to the binary (CMake copies it there; `cmake --install` places it beside the installed sandbox). A missing copy is an error in smoke mode even if a BMP exists in the source tree. In an interactive run a missing BMP logs where it looked and falls back to a generated checkerboard. The HUD is skipped in smoke so the dummy video driver does not need debug text, and the logical/window/pixel size line is skipped so smoke output stays deterministic. The SDL window→logical converter is interactive-only (`SDL_RenderCoordinatesFromWindow` needs a renderer).

Room `--smoke` checks `aabb_overlap` / `aabb_move`, walks a scripted key→door win (without touching the hazard) and a walk-into-hazard fail/reset, then runs the same few dummy-video ticks. CMake copies `assets/room_atlas.bmp` next to `midas_room`; if that copy is missing or `load_bmp` fails, the room draws solid rects instead of failing smoke. `ctest` runs both.

See [docs/BUILDING.md](docs/BUILDING.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), and [docs/GAME_V0.md](docs/GAME_V0.md).

## What's next (2D)

Midas stays a small 2D engine. Natural follow-ons, not this tree:

- **Physics** — `aabb_overlap` / `aabb_move` is what the room uses (slide along walls). Continuous collision, rotation, and a solver can wait. `Transform` is still position + scale.
- **Audio** — SDL3 can play samples without SDL_mixer. An engine audio stub can wait until a game needs it.
- **Assets** — BMP + CPU RGBA are enough to learn uploads. PNG (SDL_image) and linear filtering stay opt-in later.

Not in scope: 3D, ECS, an editor, or multiplayer. Chase AI, HP, a second room, loot, a generator, and pathfinding are out of game v0.
