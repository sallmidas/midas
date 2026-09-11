#pragma once

#include <midas/Renderer.hpp>

#include <SDL3/SDL.h>

struct midas::Renderer::Native {
    SDL_Renderer* renderer{nullptr};
};
