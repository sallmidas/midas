#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace midas {

class Renderer;

/// OS window. Native SDL handles stay in `Native` (engine-private).
///
/// `width()` / `height()` are the live client size in window coordinates and
/// can change when the user resizes. Drawing, the camera, and mouse positions
/// use `Renderer::logical_width/height` (the letterboxed `EngineConfig` size),
/// not these — after a resize they often differ (black bars).
class Window {
public:
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
    ~Window();

    [[nodiscard]] std::string_view title() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

private:
    friend class Engine;
    friend class Renderer;

    Window(std::string title, int width, int height);

    struct Native;
    Native* native() noexcept;
    const Native* native() const noexcept;

    void sync_size_from_native() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
