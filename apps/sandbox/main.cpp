#include <midas/midas.hpp>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

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

std::optional<std::filesystem::path> find_asset(const char* argv0, std::string_view name) {
    std::filesystem::path candidates[2];
    std::size_t count = 0;

    if (argv0 != nullptr && argv0[0] != '\0') {
        candidates[count++] = std::filesystem::path(argv0).parent_path() / "assets" / name;
    }

    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        candidates[count++] = cwd / "assets" / name;
    }

    for (std::size_t i = 0; i < count; ++i) {
        ec.clear();
        if (std::filesystem::is_regular_file(candidates[i], ec)) {
            return candidates[i];
        }
    }
    return std::nullopt;
}

midas::Texture load_sprite(midas::Renderer& renderer, const char* argv0) {
    constexpr int kSize = 64;
    constexpr midas::Color kDark{0.25f, 0.18f, 0.08f, 1.0f};

    if (const auto path = find_asset(argv0, "midas_sprite.bmp")) {
        return renderer.load_bmp(path->string());
    }

    const auto pixels = midas::make_checkerboard_rgba(kSize, kSize, midas::Color::gold(), kDark, 8);
    return renderer.create_texture(kSize, kSize, pixels);
}

void apply_camera_controls(midas::Engine& engine, midas::Camera& camera, const midas::Camera& home) {
    const auto& input = engine.input();
    const float dt = static_cast<float>(engine.time().delta_seconds());
    const float viewport_w = static_cast<float>(engine.window().width());
    const float viewport_h = static_cast<float>(engine.window().height());
    const midas::Vec2 view_center{viewport_w * 0.5f, viewport_h * 0.5f};

    if (input.key_pressed(midas::Key::Space)) {
        camera = home;
        return;
    }

    midas::Vec2 move{};
    if (input.key_down(midas::Key::A) || input.key_down(midas::Key::Left)) {
        move.x -= 1.0f;
    }
    if (input.key_down(midas::Key::D) || input.key_down(midas::Key::Right)) {
        move.x += 1.0f;
    }
    if (input.key_down(midas::Key::W) || input.key_down(midas::Key::Up)) {
        move.y -= 1.0f;
    }
    if (input.key_down(midas::Key::S) || input.key_down(midas::Key::Down)) {
        move.y += 1.0f;
    }

    const float length = std::hypot(move.x, move.y);
    if (length > 0.0f) {
        constexpr float pan_speed = 480.0f;
        camera.position += (move / length) * (pan_speed / camera.zoom) * dt;
    }

    if (input.mouse_down(midas::MouseButton::Right)) {
        camera.position -= input.mouse_delta() / camera.zoom;
    }

    float zoom_dir = 0.0f;
    if (input.key_down(midas::Key::E)) {
        zoom_dir += 1.0f;
    }
    if (input.key_down(midas::Key::Q)) {
        zoom_dir -= 1.0f;
    }
    if (zoom_dir != 0.0f) {
        camera.zoom_toward(view_center, std::exp(zoom_dir * 0.9f * dt), viewport_w, viewport_h);
    }

    if (input.wheel_y() != 0.0f) {
        camera.zoom_toward(input.mouse_position(), std::pow(1.12f, input.wheel_y()), viewport_w,
                           viewport_h);
    }
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

        constexpr float square = 160.0f;
        const float center_x = (static_cast<float>(engine.window().width()) - square) * 0.5f;
        const float center_y = (static_cast<float>(engine.window().height()) - square) * 0.5f;

        midas::Texture sprite = load_sprite(engine.renderer(), argc > 0 ? argv[0] : nullptr);

        midas::Entity gold;
        gold.transform.position = {center_x, center_y};
        gold.size = {square, square};
        gold.color = midas::Color::gold();

        midas::Entity tile;
        tile.transform.position = {center_x + square + 48.0f, center_y};
        tile.size = {square, square};
        tile.texture = &sprite;
        tile.color = midas::Color::white();

        const midas::Camera home = engine.renderer().camera();
        midas::Camera camera = home;

        return engine.run([&](midas::Engine& engine) {
            if (engine.input().key_pressed(midas::Key::Escape)) {
                engine.request_quit();
                return;
            }

            apply_camera_controls(engine, camera, home);
            engine.renderer().set_camera(camera);

            auto& renderer = engine.renderer();
            renderer.clear(midas::Color::charcoal());
            midas::draw_entity(renderer, gold);
            midas::draw_entity(renderer, tile);
            renderer.present();
        });
    } catch (const std::exception& ex) {
        std::cerr << "Midas: " << ex.what() << '\n';
        return 1;
    }
}
