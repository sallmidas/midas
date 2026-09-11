#pragma once

#include <midas/Window.hpp>

#include <SDL3/SDL.h>

struct midas::Window::Native {
    SDL_Window* window{nullptr};
};
