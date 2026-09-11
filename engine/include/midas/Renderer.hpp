#pragma once

#include <midas/Types.hpp>

#include <memory>

namespace midas {

class Window;

/// 2D present path: clear, fill an axis-aligned rect, present.
class Renderer {
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    ~Renderer();

    void clear(const Color& color);
    void fill_rect(const Rect& rect, const Color& color);
    void present();

private:
    friend class Engine;

    explicit Renderer(Window& window);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
