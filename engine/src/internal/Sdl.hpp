#pragma once

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace midas::detail {

[[noreturn]] inline void throw_sdl(const char* what) {
    const char* error = SDL_GetError();
    throw std::runtime_error(std::string(what) + ": " + (error ? error : "unknown SDL error"));
}

}  // namespace midas::detail
