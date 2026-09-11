#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace midas::detail {

[[noreturn]] inline void throw_sdl(const std::string& what) {
    const char* error = SDL_GetError();
    const char* detail = (error != nullptr && error[0] != '\0') ? error : "unknown SDL error";
    throw std::runtime_error(what + ": " + detail);
}

struct SurfaceDeleter {
    void operator()(SDL_Surface* surface) const noexcept {
        if (surface != nullptr) {
            SDL_DestroySurface(surface);
        }
    }
};

using UniqueSurface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

}  // namespace midas::detail
