#include <midas/midas.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int parse_smoke_ticks(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--smoke") {
            return 3;
        }
    }

    if (const char* env = std::getenv("MIDAS_SMOKE_FRAMES")) {
        try {
            const int frames = std::stoi(env);
            if (frames > 0) {
                return frames;
            }
        } catch (const std::exception&) {
            std::cerr << "Midas: ignoring invalid MIDAS_SMOKE_FRAMES\n";
        }
    }

    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        midas::EngineConfig config;
        config.title = "Midas";
        config.width = 1280;
        config.height = 720;
        config.max_ticks = parse_smoke_ticks(argc, argv);

        midas::Engine engine{std::move(config)};

        return engine.run([](midas::Engine& engine) {
            if (engine.input().key_pressed(midas::Key::Escape)) {
                engine.request_quit();
                return;
            }

            auto& renderer = engine.renderer();
            const auto& window = engine.window();

            renderer.clear(midas::Color::charcoal());

            constexpr float size = 160.0f;
            const midas::Rect gold_square{
                (static_cast<float>(window.width()) - size) * 0.5f,
                (static_cast<float>(window.height()) - size) * 0.5f,
                size,
                size,
            };
            renderer.fill_rect(gold_square, midas::Color::gold());
            renderer.present();
        });
    } catch (const std::exception& ex) {
        std::cerr << "Midas: " << ex.what() << '\n';
        return 1;
    }
}
