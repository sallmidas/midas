#pragma once

#include <midas/Input.hpp>
#include <midas/Renderer.hpp>
#include <midas/Time.hpp>
#include <midas/Window.hpp>

#include <functional>
#include <memory>
#include <string>

namespace midas {

struct EngineConfig {
    std::string title{"Midas"};
    int width{1280};
    int height{720};
    int max_ticks{0};
};

class Engine {
public:
    explicit Engine(EngineConfig config = {});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    [[nodiscard]] Window& window() noexcept;
    [[nodiscard]] const Window& window() const noexcept;
    [[nodiscard]] Input& input() noexcept;
    [[nodiscard]] const Input& input() const noexcept;
    [[nodiscard]] Time& time() noexcept;
    [[nodiscard]] const Time& time() const noexcept;
    [[nodiscard]] Renderer& renderer() noexcept;
    [[nodiscard]] const Renderer& renderer() const noexcept;

    void request_quit() noexcept;
    [[nodiscard]] bool is_running() const noexcept;

    int run(std::function<void(Engine&)> on_tick);

private:
    void pump_events();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace midas
