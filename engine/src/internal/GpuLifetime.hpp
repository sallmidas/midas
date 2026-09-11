#pragma once

namespace midas::detail {

/// Shared "is the GPU renderer still alive?" flag.
///
/// `Renderer` holds one `shared_ptr`; each `Texture` copies it. Teardown is:
///
/// 1. Game drops textures → `SDL_DestroyTexture` while the renderer lives.
/// 2. `Renderer` sets `alive = false`, then `SDL_DestroyRenderer` (SDL also
///    frees leftover GPU textures).
/// 3. A stray `Texture` destructor sees `!alive` and skips `SDL_DestroyTexture`
///    (calling it after step 2 would be a use-after-free).
/// 4. `Window` destructor, then `SDL_Quit`.
///
/// Prefer step 1 (declare `Engine` first, then textures). This flag is the
/// seatbelt when declaration order is reversed.
struct GpuLifetime {
    bool alive{true};
};

}  // namespace midas::detail
