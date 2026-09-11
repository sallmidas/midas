# Midas

Midas is a C++20 game engine. The public API covers a window, input, a fixed 60 Hz loop, a 2D renderer with textures and an orthographic camera, and lightweight entity/transform helpers. The sandbox draws a gold square plus a textured quad you can pan and zoom.

## Layout

```
engine/include/midas/   Public headers (Engine, Window, Input, Time, Renderer,
                        Texture, Camera, Entity). SDL stays out of these files.
engine/src/             Engine implementation (SDL3 kept private)
apps/sandbox/           Demo: gold square, textured sprite, camera, Esc/quit
apps/sandbox/assets/    Demo BMP copied next to the sandbox binary
docs/                   Architecture and macOS Apple Silicon build notes
cmake/                  Homebrew SDL3 discovery, FetchContent fallback
```

## Quick start (macOS Apple Silicon)

```bash
brew install cmake ninja sdl3
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/midas_sandbox
```

A `release` preset is the same commands with `release` instead of `debug`.

If SDL3 is not installed, CMake fetches **SDL3 3.4.16** automatically.

**Esc** or close the window to quit. **WASD** / arrows (or right-drag) pan, **Q**/**E** or the mouse wheel zoom, **Space** resets the camera.

See [docs/BUILDING.md](docs/BUILDING.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
