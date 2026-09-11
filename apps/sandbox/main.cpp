#include <midas/midas.hpp>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr float kViewportW = 1280.0f;
constexpr float kViewportH = 720.0f;

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

/// Header-only math: camera invertibility, zoom clamps, AABB, transform compose.
/// Runs before SDL so a broken Camera/Rect cannot hide behind a missing GPU.
void self_check_math() {
    using midas::Camera;
    using midas::Entity;
    using midas::Rect;
    using midas::Transform;
    using midas::Vec2;

    Camera camera;
    camera.position = {kViewportW * 0.5f, kViewportH * 0.5f};
    camera.zoom = 1.0f;

    const Vec2 world{240.0f, 90.0f};
    const Vec2 roundtrip =
        camera.screen_to_world(camera.world_to_screen(world, kViewportW, kViewportH), kViewportW, kViewportH);
    if (std::abs(roundtrip.x - world.x) > 0.01f || std::abs(roundtrip.y - world.y) > 0.01f) {
        throw std::runtime_error("Midas self-check: camera world/screen roundtrip failed");
    }

    const Rect visible = camera.visible_world_rect(kViewportW, kViewportH);
    if (std::abs(visible.w - kViewportW) > 0.01f || std::abs(visible.h - kViewportH) > 0.01f) {
        throw std::runtime_error("Midas self-check: identity camera should show the full viewport");
    }
    const float aspect = visible.w / visible.h;
    if (std::abs(aspect - (kViewportW / kViewportH)) > 0.001f) {
        throw std::runtime_error("Midas self-check: visible world aspect should match the viewport");
    }

    camera.zoom = 0.0f;
    if (std::abs(camera.clamped_zoom() - 1.0f) > 0.0f) {
        throw std::runtime_error("Midas self-check: non-positive zoom should sanitize to 1");
    }
    camera.zoom = 100.0f;
    camera.clamp_zoom();
    if (camera.zoom != Camera::max_zoom) {
        throw std::runtime_error("Midas self-check: zoom was not clamped to max_zoom");
    }

    camera.zoom = 1.0f;
    camera.position = {kViewportW * 0.5f, kViewportH * 0.5f};
    const Vec2 cursor{kViewportW * 0.25f, kViewportH * 0.25f};
    const Vec2 under_cursor = camera.screen_to_world(cursor, kViewportW, kViewportH);
    camera.zoom_toward(cursor, 2.0f, kViewportW, kViewportH);
    const Vec2 still_under = camera.screen_to_world(cursor, kViewportW, kViewportH);
    if (std::abs(still_under.x - under_cursor.x) > 0.05f ||
        std::abs(still_under.y - under_cursor.y) > 0.05f) {
        throw std::runtime_error("Midas self-check: zoom_toward should keep the cursor over the same world point");
    }
    camera.zoom_toward(cursor, 0.0f, kViewportW, kViewportH);
    if (camera.zoom != 2.0f) {
        throw std::runtime_error("Midas self-check: non-positive zoom multiplier should be ignored");
    }

    const Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    const Rect b{5.0f, 5.0f, 10.0f, 10.0f};
    const Rect c{20.0f, 20.0f, 4.0f, 4.0f};
    if (!a.overlaps(b) || b.overlaps(c) || !a.contains({1.0f, 1.0f}) || a.contains({10.0f, 10.0f})) {
        throw std::runtime_error("Midas self-check: AABB overlaps/contains failed");
    }

    const Transform parent{{100.0f, 50.0f}, {2.0f, 3.0f}};
    const Transform local{{10.0f, 4.0f}, {0.5f, 0.5f}};
    const Transform world_tf = parent.then(local);
    if (std::abs(world_tf.position.x - 120.0f) > 0.01f || std::abs(world_tf.position.y - 62.0f) > 0.01f ||
        std::abs(world_tf.scale.x - 1.0f) > 0.01f || std::abs(world_tf.scale.y - 1.5f) > 0.01f) {
        throw std::runtime_error("Midas self-check: Transform::then compose failed");
    }

    Entity left;
    left.transform.position = {0.0f, 0.0f};
    left.size = {8.0f, 8.0f};
    Entity right;
    right.transform.position = {7.0f, 0.0f};
    right.size = {8.0f, 8.0f};
    if (!left.overlaps(right)) {
        throw std::runtime_error("Midas self-check: entity AABB overlap failed");
    }
}

void add_candidate(std::vector<std::filesystem::path>& candidates, std::filesystem::path path) {
    if (path.empty()) {
        return;
    }
    candidates.push_back(std::move(path));
}

std::optional<std::filesystem::path> find_asset(const std::filesystem::path& exe_dir,
                                                const char* argv0,
                                                std::string_view name) {
    std::vector<std::filesystem::path> candidates;
    add_candidate(candidates, exe_dir / "assets" / name);

    if (argv0 != nullptr && argv0[0] != '\0') {
        add_candidate(candidates, std::filesystem::path(argv0).parent_path() / "assets" / name);
    }

    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        add_candidate(candidates, cwd / "assets" / name);
        add_candidate(candidates, cwd / "apps" / "sandbox" / "assets" / name);
    }

    for (const auto& candidate : candidates) {
        ec.clear();
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return std::filesystem::weakly_canonical(candidate, ec);
        }
    }
    return std::nullopt;
}

struct LoadedSprite {
    midas::Texture texture;
    bool from_bmp{false};
    std::filesystem::path path;
};

LoadedSprite load_sprite(midas::Engine& engine, const char* argv0) {
    constexpr int kSize = 64;
    auto& renderer = engine.renderer();

    if (const auto path = find_asset(engine.executable_directory(), argv0, "midas_sprite.bmp")) {
        midas::Texture texture = renderer.load_bmp(path->string());
        return LoadedSprite{std::move(texture), true, *path};
    }

    std::cerr << "Midas: missing assets/midas_sprite.bmp\n"
              << "  looked next to the executable (" << (engine.executable_directory() / "assets")
              << "), argv0, ./assets, and ./apps/sandbox/assets\n"
              << "  using a generated 64x64 checkerboard so the sprite path still runs\n";

    const auto pixels =
        midas::make_checkerboard_rgba(kSize, kSize, midas::Color::gold(), midas::Color::bronze(), 8);
    return LoadedSprite{renderer.create_texture(kSize, kSize, pixels), false, {}};
}

midas::Entity make_fill(midas::Vec2 position, midas::Vec2 size, midas::Color color) {
    midas::Entity entity;
    entity.transform.position = position;
    entity.size = size;
    entity.color = color;
    return entity;
}

midas::Entity make_sprite(midas::Vec2 position, midas::Vec2 size, const midas::Texture& texture,
                          midas::Color tint) {
    midas::Entity entity;
    entity.transform.position = position;
    entity.size = size;
    entity.texture = &texture;
    entity.color = tint;
    return entity;
}

std::vector<midas::Entity> build_demo_scene(const midas::Texture& sprite, float viewport_w,
                                            float viewport_h) {
    // World origin is the top-left of the default view. The camera starts at the
    // viewport center, so this layout is what you see before WASD/zoom.
    const midas::Vec2 center{viewport_w * 0.5f, viewport_h * 0.5f};
    constexpr float tile = 48.0f;
    constexpr float sprite_px = 128.0f;

    std::vector<midas::Entity> scene;

    // Bronze floor strip — a row of AABB tiles the camera can pan across.
    for (int i = -6; i <= 6; ++i) {
        const float x = center.x + static_cast<float>(i) * (tile + 6.0f) - tile * 0.5f;
        const float y = center.y + 96.0f;
        const midas::Color color = (i % 2 == 0) ? midas::Color::bronze() : midas::Color::gold();
        scene.push_back(make_fill({x, y}, {tile, tile * 0.45f}, color));
    }

    // Solid gold plinth (no texture) vs the BMP sprite vs a gold-tinted copy.
    scene.push_back(make_fill({center.x - 280.0f, center.y - 80.0f}, {160.0f, 160.0f}, midas::Color::gold()));
    scene.push_back(make_sprite({center.x - sprite_px * 0.5f, center.y - 88.0f}, {sprite_px, sprite_px},
                                sprite, midas::Color::white()));
    scene.push_back(make_sprite({center.x + 120.0f, center.y - 88.0f}, {sprite_px, sprite_px}, sprite,
                                midas::Color::gold()));

    // Child-scale example: half-size sprite, composed from a parent offset.
    midas::Transform parent;
    parent.position = {center.x + 280.0f, center.y - 40.0f};
    midas::Transform local;
    local.scale = {0.5f, 0.5f};
    midas::Entity small = make_sprite({0.0f, 0.0f}, {sprite_px, sprite_px}, sprite, midas::Color::white());
    small.transform = parent.then(local);
    scene.push_back(small);

    // Distant markers so zooming out / panning has something to find.
    scene.push_back(make_fill({64.0f, 64.0f}, {36.0f, 36.0f}, midas::Color::gold()));
    scene.push_back(make_fill({viewport_w - 100.0f, 64.0f}, {36.0f, 36.0f}, midas::Color::gold()));
    scene.push_back(make_fill({64.0f, viewport_h - 100.0f}, {36.0f, 36.0f}, midas::Color::bronze()));
    scene.push_back(
        make_fill({viewport_w - 100.0f, viewport_h - 100.0f}, {36.0f, 36.0f}, midas::Color::bronze()));

    return scene;
}

void apply_camera_controls(midas::Engine& engine, midas::Camera& camera, const midas::Camera& home) {
    const auto& input = engine.input();
    const float dt = static_cast<float>(engine.time().delta_seconds());
    const float viewport_w = static_cast<float>(engine.renderer().logical_width());
    const float viewport_h = static_cast<float>(engine.renderer().logical_height());
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

    // Constant *screen-space* pan: divide by zoom so the view doesn't crawl
    // when zoomed in or race when zoomed out.
    constexpr float pan_speed = 520.0f;
    camera.position += move.normalized_or_zero() * (pan_speed / camera.clamped_zoom()) * dt;

    if (input.mouse_down(midas::MouseButton::Right)) {
        camera.position -= input.mouse_delta() / camera.clamped_zoom();
    }

    float zoom_dir = 0.0f;
    if (input.key_down(midas::Key::E)) {
        zoom_dir += 1.0f;
    }
    if (input.key_down(midas::Key::Q)) {
        zoom_dir -= 1.0f;
    }
    if (zoom_dir != 0.0f) {
        constexpr float zoom_rate = 1.05f;  // nepers / second → about 2.9× per second
        camera.zoom_toward(view_center, std::exp(zoom_dir * zoom_rate * dt), viewport_w, viewport_h);
    }

    if (input.wheel_y() != 0.0f) {
        constexpr float wheel_step = 1.15f;
        camera.zoom_toward(input.mouse_position(), std::pow(wheel_step, input.wheel_y()), viewport_w,
                           viewport_h);
    }

    camera.sanitize();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        self_check_math();

        const int smoke_ticks = parse_smoke_ticks(argc, argv);

        midas::EngineConfig config;
        config.title = "Midas";
        config.width = 1280;
        config.height = 720;
        config.max_ticks = smoke_ticks;

        midas::Engine engine{std::move(config)};

        LoadedSprite sprite = load_sprite(engine, argc > 0 ? argv[0] : nullptr);
        if (smoke_ticks > 0 && !sprite.from_bmp) {
            std::cerr << "Midas: --smoke requires assets/midas_sprite.bmp next to the binary\n";
            return 1;
        }
        if (sprite.from_bmp) {
            std::cerr << "Midas: loaded BMP " << sprite.texture.width() << "x" << sprite.texture.height()
                      << " from " << sprite.path << '\n';
        }

        const float viewport_w = static_cast<float>(engine.renderer().logical_width());
        const float viewport_h = static_cast<float>(engine.renderer().logical_height());
        const std::vector<midas::Entity> scene = build_demo_scene(sprite.texture, viewport_w, viewport_h);

        const midas::Camera home = engine.renderer().camera();
        midas::Camera camera = home;

        const int status = engine.run([&](midas::Engine& engine) {
            if (engine.input().key_pressed(midas::Key::Escape)) {
                engine.request_quit();
                return;
            }

            apply_camera_controls(engine, camera, home);
            engine.renderer().set_camera(camera);

            auto& renderer = engine.renderer();
            renderer.clear(midas::Color::charcoal());
            for (const auto& entity : scene) {
                midas::draw_entity(renderer, entity);
            }
            renderer.present();
        });

        if (smoke_ticks > 0) {
            std::cerr << "Midas: smoke completed " << smoke_ticks << " ticks (" << scene.size()
                      << " entities, camera/AABB self-check ok)\n";
        }
        return status;
    } catch (const std::exception& ex) {
        std::cerr << "Midas: " << ex.what() << '\n';
        return 1;
    }
}
