# Midas

Midas is a C++20 game engine. The public API covers a window, input, a fixed 60 Hz loop, a 2D renderer with textures and an orthographic camera, and lightweight entity/transform helpers. The sandbox draws a small gold-and-bronze scene (solid fills, a BMP sprite, a tinted sprite, and a scaled child) that you can pan and zoom.

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
| Mouse wheel | Zoom toward the cursor |
| **Space** | Reset pan and zoom |

Zoom is uniform and clamped to **0.25–8**. The view is aspect-correct: the visible world matches the 1280×720 logical window.

Headless smoke (a few 60 Hz ticks, then exit):

```bash
SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_sandbox --smoke
# or: ctest --test-dir build/debug --output-on-failure
```

`--smoke` also runs camera/AABB self-checks and **requires** `assets/midas_sprite.bmp` next to the binary (CMake copies it there). A missing BMP is an error in smoke mode; in an interactive run it logs where it looked and falls back to a generated checkerboard.

See [docs/BUILDING.md](docs/BUILDING.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
