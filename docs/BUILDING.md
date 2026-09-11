# Building Midas (macOS and Linux)

The engine is C++20. SDL3 is C, so the top-level CMake project enables both languages. Debug and release Ninja presets are the supported build paths on **macOS** (Apple Silicon or Intel) and **Linux**.

## Prerequisites

### macOS (Apple Silicon / Intel)

Install Xcode Command Line Tools, then:

```bash
xcode-select --install
brew install cmake ninja sdl3
```

- **CMake** 3.21 or newer (presets version 3)
- **Ninja** (the `debug` and `release` preset generator)
- **SDL3** via Homebrew (`brew install sdl3`), typically under `/opt/homebrew` (Apple Silicon) or `/usr/local` (Intel)

Apple clang from Command Line Tools is enough (`c++` / `clang++` with `-std=c++20`).

### Linux (Debian / Ubuntu)

```bash
sudo apt install cmake ninja-build g++ pkg-config
```

`clang` works as well. If `c++` on `PATH` is Clang but CMake cannot link `libstdc++` (a toolchain mismatch with GCC 14 internals), configure with GCC explicitly:

```bash
CC=gcc CXX=g++ cmake --preset debug
```

SDL3 is optional: many distros still ship only SDL2, in which case CMake downloads SDL3 via FetchContent. If your distro has SDL3 3.x:

```bash
sudo apt install libsdl3-dev
```

Headless / CI machines do not need a display server. FetchContent sets `SDL_UNIX_CONSOLE_BUILD` on Linux so SDL3 can build with the dummy video driver.

### SDL3 via FetchContent

If you skip an installed SDL3, configure still succeeds: CMake downloads **SDL3 3.4.16** the first time (network required). Later configures reuse the build tree.

## Configure and build

From the repository root, same commands on macOS and Linux:

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

The sandbox clears to charcoal and draws a small gold/bronze scene: a tiled floor strip, a solid gold square, the BMP sprite, a gold-tinted copy of that sprite, a half-scale child sprite (`Transform::then`), and corner markers so pan/zoom has landmarks.

| Input | Action |
| --- | --- |
| **Esc** or close the window | Quit |
| **WASD** / arrow keys | Pan the camera (screen-space speed is constant across zoom) |
| Right mouse drag | Pan the camera |
| **Q** / **E** | Zoom out / in around the view center (clamped to 0.25–8) |
| Mouse wheel | Zoom toward the cursor (same clamp) |
| **Space** | Reset pan and zoom |

Textures use **nearest-neighbor** sampling so the BMP stays sharp when zoomed.

Headless / CI smoke (a few 60 Hz ticks, then exit). This path also runs camera and AABB self-checks and fails if the BMP was not copied next to the binary:

```bash
SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_sandbox --smoke
ctest --test-dir build/debug --output-on-failure
```

`MIDAS_SMOKE_FRAMES` is an alternative to `--smoke` (must be a positive integer).

If you run the sandbox **without** `--smoke` and the BMP is missing, it prints every directory it searched and uploads a generated checkerboard instead. That fallback is logged; it is not silent.

## CMake presets

| Preset | Build type | Generator | Binary dir |
| --- | --- | --- | --- |
| `debug` | Debug | Ninja | `build/debug` |
| `release` | Release | Ninja | `build/release` |

Both presets set `CMAKE_EXPORT_COMPILE_COMMANDS=ON`. Example without presets:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

On Linux, the sandbox `BUILD_RPATH` is `$ORIGIN` so a shared libSDL3 placed next to the binary can be found. On macOS it includes `@executable_path` plus Homebrew prefixes.

## Homebrew vs FetchContent

`cmake/MidasSDL3.cmake` prefers an installed SDL3 config package:

1. `brew --prefix sdl3` when Homebrew is on `PATH` (macOS, or Linuxbrew)
2. `/opt/homebrew` (Apple Silicon default prefix)
3. `/usr/local` (Intel Homebrew / manual installs)
4. `find_package(SDL3)` on the default CMake prefix (Linux `libsdl3-dev`)
5. FetchContent of [SDL 3.4.16](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16)

Reconfigure after `brew install sdl3` (or `apt install libsdl3-dev`) so CMake can pick up the keg / package instead of the pinned tarball.
