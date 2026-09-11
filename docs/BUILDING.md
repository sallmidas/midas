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

SDL3 is optional: many distros still ship only SDL2, in which case CMake downloads SDL3 via FetchContent. If your distro has SDL3 **3.2 or newer** (needed for the debug HUD):

```bash
sudo apt install libsdl3-dev
```

Headless / CI machines do not need a display server. FetchContent sets `SDL_UNIX_CONSOLE_BUILD` on Linux so SDL3 can build with the dummy video driver. A windowed run needs an SDL video backend (X11 or Wayland): install `libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev` (and friends) before the first configure, or use distro `libsdl3-dev`. If CMake already fetched SDL3 without those packages, wipe `build/` and reconfigure.

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

CMake copies `apps/sandbox/assets/midas_sprite.bmp` next to that binary (`build/debug/bin/assets/`). Changing the BMP recopies it even if the sandbox did not relink. `cmake --install --prefix <prefix>` puts `midas_sandbox` in `<prefix>/bin` and the BMP in `<prefix>/bin/assets`.

Release is the same flow with optimizations:

```bash
cmake --preset release
cmake --build --preset release
./build/release/bin/midas_sandbox
```

`compile_commands.json` is generated in `build/debug/` (and `build/release/`) for clangd.

## Run

The sandbox clears to charcoal and draws a small gold/bronze scene: a tiled floor strip, a solid gold square (easing toward bronze with `Color::lerp`), the BMP sprite, a gold-tinted copy of that sprite, a half-scale child sprite (`Transform::then`), and corner markers so pan/zoom has landmarks. A screen-space HUD (top-left) shows the fixed 60 Hz `dt`, wall-clock FPS, camera, and a one-line WASD/zoom/Space/F1 legend — it does not pan with the world. The banner padding uses `Rect::expanded`. **F1** or **`** hides it (handy for demos). `--smoke` leaves the HUD off so the dummy driver never has to draw debug text. The window is resizable and high-DPI (`SDL_WINDOW_HIGH_PIXEL_DENSITY`); the 1280×720 **logical present** view is letterboxed. Camera and mouse use that logical size, not live window coordinates and not framebuffer pixels. Wheel zoom uses the closed present rect (`contains_inclusive`) so only letterbox bars are skipped. Interactive mode logs logical vs window vs pixel size once at startup.

| Input | Action |
| --- | --- |
| **Esc** or close the window | Quit |
| **WASD** / arrow keys | Pan the camera (screen-space speed is constant across zoom) |
| Right mouse drag | Pan the camera |
| **Q** / **E** | Zoom out / in around the view center (clamped to 0.25–8) |
| Mouse wheel | Zoom toward the cursor (same clamp; ignored in letterbox bars, not at the far edge of the view) |
| **Space** | Reset pan and zoom to the identity logical view |
| **F1** or **`** | Toggle the debug HUD |

Textures use **nearest-neighbor** sampling so the BMP stays sharp when zoomed.

Headless / CI smoke (a few 60 Hz ticks, then exit). This path also runs camera, AABB, `Rect::expanded`/`inset`/`contains_inclusive`, `Transform::then`, `Color::lerp`, `clamp`/`clamp01`, `Vec2::length_squared`, `Cooldown`, and the three-space mouse policy self-checks and fails if the BMP was not copied next to the binary. It does **not** require the debug HUD, does **not** print the interactive logical/window/pixel size line, and does **not** call `SDL_RenderCoordinatesFromWindow` (that converter needs a renderer and stays interactive-only):

```bash
SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_sandbox --smoke
ctest --preset debug
# equivalent: ctest --test-dir build/debug --output-on-failure
```

`MIDAS_SMOKE_FRAMES` is an alternative to `--smoke` (must be a positive integer).

If you run the sandbox **without** `--smoke` and the BMP is missing, it prints every directory it searched and uses a generated checkerboard instead. That fallback is logged; it is not silent.

## CMake presets

| Preset | Build type | Generator | Binary dir |
| --- | --- | --- | --- |
| `debug` | Debug | Ninja | `build/debug` |
| `release` | Release | Ninja | `build/release` |

Configure and build with `cmake --preset debug` and `cmake --build --preset debug`. After a build, `ctest --preset debug` (and `ctest --preset release`) run the dummy-video sandbox smoke. Both configure presets set `CMAKE_EXPORT_COMPILE_COMMANDS=ON`. Example without presets:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

On Linux, the sandbox `BUILD_RPATH` / `INSTALL_RPATH` is `$ORIGIN` so a shared libSDL3 placed next to the binary can be found. On macOS it includes `@executable_path` plus Homebrew prefixes.

Install the sandbox and its BMP (optional):

```bash
cmake --install build/debug --prefix /tmp/midas-install
/tmp/midas-install/bin/midas_sandbox
```

## Homebrew vs FetchContent

`cmake/MidasSDL3.cmake` prefers an installed SDL3 config package:

1. `brew --prefix sdl3` when Homebrew is on `PATH` (macOS, or Linuxbrew)
2. `/opt/homebrew` (Apple Silicon default prefix)
3. `/usr/local` (Intel Homebrew / manual installs)
4. `find_package(SDL3)` on the default CMake prefix (Linux `libsdl3-dev`)
5. FetchContent of [SDL 3.4.16](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16)

Reconfigure after `brew install sdl3` (or `apt install libsdl3-dev`) so CMake can pick up the keg / package instead of the pinned tarball.
