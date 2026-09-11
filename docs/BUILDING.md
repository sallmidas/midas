# Building Midas (macOS Apple Silicon)

The engine is C++20. SDL3 is C, so the top-level CMake project enables both languages. The supported developer setup is **macOS on Apple Silicon** (M-series) with Homebrew.

## Prerequisites

Install Xcode Command Line Tools, then:

```bash
xcode-select --install
brew install cmake ninja sdl3
```

- **CMake** 3.21 or newer (presets version 3)
- **Ninja** (the `debug` and `release` preset generator)
- **SDL3** via Homebrew (`brew install sdl3`), typically under `/opt/homebrew`

Apple clang from Command Line Tools is enough (`c++` / `clang++` with `-std=c++20`).

If you skip `sdl3`, configure still succeeds: CMake downloads **SDL3 3.4.16** with FetchContent.

## Configure and build

From the repository root:

```bash
cmake --preset debug
cmake --build --preset debug
```

The debug tree is `build/debug/`. The sandbox binary is:

```bash
./build/debug/bin/midas_sandbox
```

CMake copies `apps/sandbox/assets/` next to that binary (`build/debug/bin/assets/`), including the demo BMP sprite.

Release is the same flow with optimizations:

```bash
cmake --preset release
cmake --build --preset release
./build/release/bin/midas_sandbox
```

`compile_commands.json` is generated in `build/debug/` (and `build/release/`) for clangd.

## Run

The sandbox clears to charcoal, draws a solid gold square, and draws a textured checkerboard quad beside it.

| Input | Action |
| --- | --- |
| **Esc** or close the window | Quit |
| **WASD** / arrow keys | Pan the camera |
| Right mouse drag | Pan the camera |
| **Q** / **E** | Zoom out / in (view center) |
| Mouse wheel | Zoom toward the cursor |
| **Space** | Reset pan and zoom |

Headless / CI smoke (a few 60 Hz ticks, then exit):

```bash
SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_sandbox --smoke
```

`MIDAS_SMOKE_FRAMES` is an alternative to `--smoke` (must be a positive integer). If the BMP is missing, the sandbox generates the same checkerboard on the CPU and uploads it with `create_texture`.

## CMake presets

| Preset | Build type | Generator | Binary dir |
| --- | --- | --- | --- |
| `debug` | Debug | Ninja | `build/debug` |
| `release` | Release | Ninja | `build/release` |

Example without presets:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

## Homebrew vs FetchContent

`cmake/MidasSDL3.cmake` prefers an installed SDL3 config package:

1. `brew --prefix sdl3` when Homebrew is on `PATH`
2. `/opt/homebrew` (Apple Silicon default prefix)
3. `/usr/local` (Intel Homebrew / manual installs)
4. FetchContent of [SDL 3.4.16](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16)

Reconfigure after `brew install sdl3` so CMake can pick up the keg instead of the pinned tarball.
