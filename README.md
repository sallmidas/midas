# Midas

Midas is a C++20 game engine. This repository is the initial scaffold: a small public API, an SDL3 window/renderer, a fixed 60 Hz loop, and a sandbox that clears the screen and draws a gold square.

## Layout

```
engine/include/midas/   Public headers (Engine, Window, Input, Time, Renderer)
engine/src/             Engine implementation (SDL3 kept private)
apps/sandbox/           Demo app: window, clear, gold square, Esc/quit
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

Press **Esc** or close the window to quit. See [docs/BUILDING.md](docs/BUILDING.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
