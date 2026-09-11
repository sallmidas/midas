# Midas

Midas is a C++20 game engine. The public API covers a window, input, a fixed 60 Hz loop, a 2D renderer with textures and an orthographic camera, and lightweight entity/transform helpers. The sandbox draws a small gold-and-bronze scene (solid fills, a BMP sprite, a tinted sprite, and a scaled child) that you can pan and zoom. When the HUD is on it includes a one-line WASD/zoom/F1 legend; press **F1** or **`** to hide it for recordings.

## Layout

```
engine/include/midas/   Public headers (Engine, Window, Input, Time, Renderer,
                        Texture, Camera, Entity). SDL stays out of these files.
engine/src/             Engine implementation (SDL3 kept private)
apps/sandbox/           Demo scene: camera, sprites, entities, Esc/quit
apps/sandbox/assets/    Demo BMP copied next to the sandbox binary
docs/                   Architecture and macOS / Linux build notes
cmake/                  Homebrew / system SDL3 discovery, FetchContent fallback
```

## Quick start

**macOS (Apple Silicon, Homebrew):**

```bash
brew install cmake ninja sdl3
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/midas_sandbox
```

**Linux:**

```bash
sudo apt install cmake ninja-build g++   # or clang
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/midas_sandbox
```

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

Zoom is uniform and clamped to **0.25–8**. The view is aspect-correct: the visible world matches the 1280×720 **logical present** size. The OS window is resizable and high-DPI; letterboxing keeps that logical view undistorted. Camera and mouse use `Renderer::logical_size()` (logical present pixels), not live `Window::width/height` (window coordinates) and not `Window::pixel_width/height` (framebuffer). Interactive mode logs those three sizes once at startup. Wheel zoom uses the closed present rect (`Rect::contains_inclusive`) so the far edges of the view still zoom — only letterbox bars are skipped.

Headless smoke (a few 60 Hz ticks, then exit):

```bash
SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_sandbox --smoke
# or: ctest --preset debug
```

`--smoke` also runs camera/AABB/`Color::lerp`/`clamp`/`Vec2::length_squared`/`Rect::expanded`/`contains_inclusive`/`Cooldown` self-checks (including the three-space mouse policy) and **requires** `assets/midas_sprite.bmp` next to the binary (CMake copies it there; `cmake --install` places it beside the installed sandbox). A missing copy is an error in smoke mode even if a BMP exists in the source tree. In an interactive run a missing BMP logs where it looked and falls back to a generated checkerboard. The HUD is skipped in smoke so the dummy video driver does not need debug text, and the logical/window/pixel size line is skipped so smoke output stays deterministic. The SDL window→logical converter is interactive-only (`SDL_RenderCoordinatesFromWindow` needs a renderer).

See [docs/BUILDING.md](docs/BUILDING.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
