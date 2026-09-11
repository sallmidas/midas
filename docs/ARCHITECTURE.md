# Midas architecture

Midas is split into a static engine library and a sandbox application. SDL3 is an implementation detail of the engine; public headers under `engine/include/midas/` do not include SDL.

## Targets

| Target | Path | Role |
| --- | --- | --- |
| `midas` | `engine/` | Core library (`midas::midas`) |
| `midas_sandbox` | `apps/sandbox/` | Windowed demo that exercises the public API |

## Public API

```
Engine     Owns the SDL video subsystem and the four subsystems below.
           Runs a capped 60 Hz tick loop and pumps OS events.
Window     OS window (size, title). Native SDL handles stay private.
Input      Keyboard state (down / pressed this tick) and quit (close box).
Time       Fixed timestep: `Time::tick_hz == 60`, `delta_seconds() == 1/60`.
Renderer   Clear, axis-aligned fill rect, present. Logical size matches the window.
```

`Types.hpp` defines `Color` and `Rect`. `midas.hpp` is an umbrella include.

Games talk only to `Engine` and the subsystem references it returns:

```cpp
midas::Engine engine({.title = "Midas", .width = 1280, .height = 720});
engine.run([](midas::Engine& e) {
    if (e.input().key_pressed(midas::Key::Escape)) {
        e.request_quit();
        return;
    }
    e.renderer().clear(midas::Color::charcoal());
    e.renderer().fill_rect(/* ... */, midas::Color::gold());
    e.renderer().present();
});
```

`EngineConfig::max_ticks` stops the loop after N ticks (used by `--smoke` / `MIDAS_SMOKE_FRAMES`). Closing the window sets `Input::quit_requested()` and ends the loop.

## Frame loop

Each tick:

1. Snapshot previous keyboard state (`key_pressed` is an edge).
2. Pump SDL events into `Input`.
3. Stop if quit was requested.
4. Invoke the tick callback (update + render).
5. Advance `Time::tick_index()`.
6. Sleep the remainder of `1/60` s so the loop holds 60 Hz even without vsync (for example `SDL_VIDEODRIVER=dummy`).

The renderer uses SDL3 logical presentation (`SDL_LOGICAL_PRESENTATION_LETTERBOX`) so drawing stays in the configured window coordinates on Retina / Apple Silicon displays.

## Dependencies

SDL3 is resolved by `cmake/MidasSDL3.cmake`:

1. **Homebrew / system** — `brew --prefix sdl3` and `/opt/homebrew` on macOS.
2. **FetchContent** — SDL3 **3.4.16** if no config package is found.

The engine links `SDL3::SDL3` and does not leak that include path into public headers.
