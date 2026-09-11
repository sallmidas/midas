#include <midas/midas.hpp>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
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

/// Header-only math: camera invertibility, zoom clamps, AABB edges,
/// Rect::expanded/inset, Transform::then identity/associativity, Color::lerp,
/// Vec2::length_squared. Runs before SDL so a broken Camera/Rect cannot hide
/// behind a missing GPU.
void self_check_math() {
    using midas::Camera;
    using midas::Entity;
    using midas::Rect;
    using midas::Transform;
    using midas::Vec2;

    const Vec2 three_four{3.0f, 4.0f};
    if (std::abs(three_four.length_squared() - 25.0f) > 0.001f ||
        std::abs(three_four.length() - 5.0f) > 0.001f ||
        Vec2{}.length_squared() != 0.0f) {
        throw std::runtime_error("Midas self-check: Vec2::length_squared failed");
    }

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

    camera.zoom = std::numeric_limits<float>::quiet_NaN();
    if (std::abs(camera.clamped_zoom() - 1.0f) > 0.0f) {
        throw std::runtime_error("Midas self-check: NaN zoom should sanitize to 1");
    }
    camera.position = {std::numeric_limits<float>::quiet_NaN(), 10.0f};
    camera.zoom = 1.0f;
    {
        const midas::Vec2 pan = camera.finite_position();
        if (pan.x != 0.0f || pan.y != 10.0f || std::isfinite(camera.position.x)) {
            throw std::runtime_error("Midas self-check: finite_position should not mutate the camera");
        }
        const Vec2 projected =
            camera.world_to_screen({0.0f, 10.0f}, kViewportW, kViewportH);
        if (!std::isfinite(projected.x) || !std::isfinite(projected.y)) {
            throw std::runtime_error("Midas self-check: projection should ignore NaN pan");
        }
    }
    camera.position = {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity()};
    camera.sanitize();
    if (camera.position.x != 0.0f || camera.position.y != 0.0f || camera.zoom != 1.0f) {
        throw std::runtime_error("Midas self-check: sanitize should drop non-finite pan/zoom");
    }

    camera.position = {kViewportW * 0.5f, kViewportH * 0.5f};
    camera.zoom = 2.0f;
    const Rect zoomed = camera.visible_world_rect(kViewportW, kViewportH);
    if (std::abs(zoomed.w - kViewportW * 0.5f) > 0.01f || std::abs(zoomed.h - kViewportH * 0.5f) > 0.01f) {
        throw std::runtime_error("Midas self-check: zoom 2 should show half the world");
    }
    if (std::abs((zoomed.w / zoomed.h) - (kViewportW / kViewportH)) > 0.001f) {
        throw std::runtime_error("Midas self-check: zoomed view should stay aspect-correct");
    }
    const Rect projected = camera.project({10.0f, 20.0f, 100.0f, 50.0f}, kViewportW, kViewportH);
    if (std::abs(projected.w - 200.0f) > 0.01f || std::abs(projected.h - 100.0f) > 0.01f) {
        throw std::runtime_error("Midas self-check: project should scale fills and sprites the same");
    }

    const Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    const Rect b{5.0f, 5.0f, 10.0f, 10.0f};
    const Rect c{20.0f, 20.0f, 4.0f, 4.0f};
    if (!a.overlaps(b) || b.overlaps(c) || !a.contains({1.0f, 1.0f}) || a.contains({10.0f, 10.0f})) {
        throw std::runtime_error("Midas self-check: AABB overlaps/contains failed");
    }

    // Half-open AABB: share an edge or a corner → no overlap. Zero-size is empty.
    const Rect right_touch{10.0f, 0.0f, 10.0f, 10.0f};
    const Rect below_touch{0.0f, 10.0f, 10.0f, 10.0f};
    const Rect inside{2.0f, 2.0f, 3.0f, 3.0f};
    const Rect empty{0.0f, 0.0f, 0.0f, 0.0f};
    if (a.overlaps(right_touch) || a.overlaps(below_touch) || right_touch.overlaps(a) ||
        a.overlaps(empty) || empty.overlaps(a) || empty.overlaps(empty) || !a.overlaps(a) ||
        !a.overlaps(inside) || !inside.overlaps(a)) {
        throw std::runtime_error("Midas self-check: AABB edge/containment/empty overlap failed");
    }
    if (!a.contains({0.0f, 0.0f}) || !a.contains({9.99f, 9.99f}) || a.contains({-0.01f, 0.0f}) ||
        empty.contains({0.0f, 0.0f})) {
        throw std::runtime_error("Midas self-check: AABB contains half-open edges failed");
    }

    const Rect pad{10.0f, 20.0f, 30.0f, 40.0f};
    const Rect grown = pad.expanded(5.0f);
    const Rect shrunk = pad.inset(5.0f);
    const Rect roundtrip = pad.expanded(8.0f).inset(8.0f);
    if (std::abs(grown.x - 5.0f) > 0.0f || std::abs(grown.y - 15.0f) > 0.0f ||
        std::abs(grown.w - 40.0f) > 0.0f || std::abs(grown.h - 50.0f) > 0.0f ||
        std::abs(shrunk.x - 15.0f) > 0.0f || std::abs(shrunk.w - 20.0f) > 0.0f ||
        std::abs(shrunk.h - 30.0f) > 0.0f || pad.inset(5.0f).x != pad.expanded(-5.0f).x ||
        std::abs(roundtrip.x - pad.x) > 0.0f || std::abs(roundtrip.y - pad.y) > 0.0f ||
        std::abs(roundtrip.w - pad.w) > 0.0f || std::abs(roundtrip.h - pad.h) > 0.0f) {
        throw std::runtime_error("Midas self-check: Rect::expanded/inset failed");
    }
    const Rect too_small = pad.inset(20.0f);
    if (too_small.w > 0.0f || too_small.contains({10.0f, 20.0f})) {
        throw std::runtime_error("Midas self-check: heavy Rect::inset should be empty");
    }

    const Transform parent{{100.0f, 50.0f}, {2.0f, 3.0f}};
    const Transform local{{10.0f, 4.0f}, {0.5f, 0.5f}};
    const Transform world_tf = parent.then(local);
    if (std::abs(world_tf.position.x - 120.0f) > 0.01f || std::abs(world_tf.position.y - 62.0f) > 0.01f ||
        std::abs(world_tf.scale.x - 1.0f) > 0.01f || std::abs(world_tf.scale.y - 1.5f) > 0.01f) {
        throw std::runtime_error("Midas self-check: Transform::then compose failed");
    }

    const Transform identity{};
    const Transform id_then_local = identity.then(local);
    const Transform parent_then_id = parent.then(identity);
    if (std::abs(id_then_local.position.x - local.position.x) > 0.01f ||
        std::abs(id_then_local.position.y - local.position.y) > 0.01f ||
        std::abs(id_then_local.scale.x - local.scale.x) > 0.01f ||
        std::abs(id_then_local.scale.y - local.scale.y) > 0.01f ||
        std::abs(parent_then_id.position.x - parent.position.x) > 0.01f ||
        std::abs(parent_then_id.position.y - parent.position.y) > 0.01f ||
        std::abs(parent_then_id.scale.x - parent.scale.x) > 0.01f ||
        std::abs(parent_then_id.scale.y - parent.scale.y) > 0.01f) {
        throw std::runtime_error("Midas self-check: Transform::then identity failed");
    }

    const Transform grandchild{{2.0f, 1.0f}, {4.0f, 0.5f}};
    const Transform assoc_left = parent.then(local.then(grandchild));
    const Transform assoc_right = parent.then(local).then(grandchild);
    if (std::abs(assoc_left.position.x - assoc_right.position.x) > 0.01f ||
        std::abs(assoc_left.position.y - assoc_right.position.y) > 0.01f ||
        std::abs(assoc_left.scale.x - assoc_right.scale.x) > 0.01f ||
        std::abs(assoc_left.scale.y - assoc_right.scale.y) > 0.01f) {
        throw std::runtime_error("Midas self-check: Transform::then should be associative");
    }

    const Vec2 local_pt{3.0f, 5.0f};
    const Vec2 nested = parent.apply(local.apply(local_pt));
    const Vec2 composed = parent.then(local).apply(local_pt);
    if (std::abs(nested.x - composed.x) > 0.01f || std::abs(nested.y - composed.y) > 0.01f) {
        throw std::runtime_error("Midas self-check: Transform::then should match nested apply");
    }

    const Transform flipped{{10.0f, 20.0f}, {-1.0f, 1.0f}};
    const Rect flipped_bounds = flipped.to_rect({8.0f, 8.0f});
    if (flipped_bounds.w >= 0.0f) {
        throw std::runtime_error("Midas self-check: negative scale should produce a non-positive AABB width");
    }

    Entity left;
    left.transform.position = {0.0f, 0.0f};
    left.size = {8.0f, 8.0f};
    Entity right;
    right.transform.position = {7.0f, 0.0f};
    right.size = {8.0f, 8.0f};
    Entity far;
    far.transform.position = {8.0f, 0.0f};
    far.size = {8.0f, 8.0f};
    if (!left.overlaps(right) || left.overlaps(far)) {
        throw std::runtime_error("Midas self-check: entity AABB overlap failed");
    }

    using midas::Color;
    const Color gold = Color::gold();
    const Color bronze = Color::bronze();
    const Color mid = Color::lerp(gold, bronze, 0.5f);
    const Color at0 = Color::lerp(gold, bronze, 0.0f);
    const Color at1 = Color::lerp(gold, bronze, 1.0f);
    const Color clamped_hi = Color::lerp(gold, bronze, 4.0f);
    const Color clamped_lo = Color::lerp(gold, bronze, -2.0f);
    const Color clamped_nan = Color::lerp(gold, bronze, std::numeric_limits<float>::quiet_NaN());
    if (std::abs(at0.r - gold.r) > 0.001f || std::abs(at0.g - gold.g) > 0.001f ||
        std::abs(at1.r - bronze.r) > 0.001f || std::abs(at1.b - bronze.b) > 0.001f ||
        std::abs(clamped_hi.r - bronze.r) > 0.001f || std::abs(clamped_lo.r - gold.r) > 0.001f ||
        std::abs(clamped_nan.r - gold.r) > 0.001f ||
        std::abs(mid.r - (gold.r + bronze.r) * 0.5f) > 0.001f ||
        std::abs(mid.a - 1.0f) > 0.001f) {
        throw std::runtime_error("Midas self-check: Color::lerp failed");
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
                                                std::string_view name, bool next_to_binary_only) {
    std::vector<std::filesystem::path> candidates;
    add_candidate(candidates, exe_dir / "assets" / name);

    if (argv0 != nullptr && argv0[0] != '\0') {
        add_candidate(candidates, std::filesystem::path(argv0).parent_path() / "assets" / name);
    }

    if (!next_to_binary_only) {
        std::error_code cwd_ec;
        const auto cwd = std::filesystem::current_path(cwd_ec);
        if (!cwd_ec) {
            add_candidate(candidates, cwd / "assets" / name);
            add_candidate(candidates, cwd / "apps" / "sandbox" / "assets" / name);
        }
    }

    for (const auto& candidate : candidates) {
        std::error_code ec;
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

LoadedSprite load_sprite(midas::Engine& engine, const char* argv0, bool smoke) {
    constexpr int kSize = 64;
    auto& renderer = engine.renderer();

    if (const auto path =
            find_asset(engine.executable_directory(), argv0, "midas_sprite.bmp", smoke)) {
        midas::Texture texture = renderer.load_bmp(path->string());
        return LoadedSprite{std::move(texture), true, *path};
    }

    if (smoke) {
        std::cerr << "Midas: --smoke requires assets/midas_sprite.bmp next to the binary\n"
                  << "  looked in " << (engine.executable_directory() / "assets").string() << '\n';
        throw std::runtime_error("smoke missing BMP (CMake should copy it next to midas_sandbox)");
    }

    std::cerr << "Midas: missing assets/midas_sprite.bmp\n"
              << "  looked next to the executable ("
              << (engine.executable_directory() / "assets").string()
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

struct DemoScene {
    std::vector<midas::Entity> entities;
    std::size_t plinth_index{0};
};

DemoScene build_demo_scene(const midas::Texture& sprite, float viewport_w, float viewport_h) {
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
    // Color::lerp pulses this fill between gold and bronze (teaching mix).
    const std::size_t plinth_index = scene.size();
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

    return DemoScene{std::move(scene), plinth_index};
}

void apply_camera_controls(midas::Engine& engine, midas::Camera& camera) {
    const auto& input = engine.input();
    const float dt = static_cast<float>(engine.time().delta_seconds());
    const midas::Vec2 viewport = engine.renderer().logical_size();
    const float viewport_w = viewport.x;
    const float viewport_h = viewport.y;
    const midas::Vec2 view_center{viewport_w * 0.5f, viewport_h * 0.5f};

    if (!input.window_focused()) {
        camera.sanitize();
        return;
    }

    // Identity view for the *current* logical viewport (not a stale copy from
    // startup). Same-frame WASD / wheel / right-drag are skipped so a reset is
    // not immediately overwritten. Sanitize drops any leftover NaN pan/zoom.
    if (input.key_pressed(midas::Key::Space)) {
        camera.position = view_center;
        camera.zoom = 1.0f;
        camera.sanitize();
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
    // when zoomed in or race when zoomed out. `dt` is the fixed 1/60 gameplay
    // step, not wall-clock `frame_seconds()` — a hitch must not fling the camera.
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

/// Screen-space HUD: stays put while the world pans/zooms. Uses fixed-timestep
/// `delta_seconds()` for the label and wall-clock `frames_per_second()` for pacing.
/// Hidden during `--smoke` (math self-check does not need debug text) and when
/// the player toggles it off with F1 or backtick. Line 3 is a one-line control
/// legend (WASD, zoom, Space, F1) so the README table is also on screen.
void draw_debug_overlay(midas::Engine& engine, const midas::Camera& camera) {
    auto& renderer = engine.renderer();
    const auto& time = engine.time();

    constexpr float x = 10.0f;
    constexpr float y = 10.0f;
    constexpr float line_h = 12.0f;
    constexpr float pad = 4.0f;
    const midas::Rect hud_text{x, y, 552.0f, 38.0f};
    renderer.fill_screen_rect(hud_text.expanded(pad), {0.0f, 0.0f, 0.0f, 0.55f});

    std::ostringstream line1;
    line1 << std::fixed << std::setprecision(4) << "Midas  dt=" << time.delta_seconds() << "s";
    const double fps = time.frames_per_second();
    if (fps > 0.0) {
        line1 << std::setprecision(0) << "  " << fps << " fps";
    }
    if (!engine.input().window_focused()) {
        line1 << "  [unfocused]";
    }

    std::ostringstream line2;
    line2 << std::fixed << std::setprecision(1) << "cam " << camera.position.x << ","
          << camera.position.y << "  zoom " << std::setprecision(2) << camera.clamped_zoom();

    renderer.draw_debug_text({x, y}, line1.str(), midas::Color::gold());
    renderer.draw_debug_text({x, y + line_h}, line2.str(), midas::Color::white());
    renderer.draw_debug_text({x, y + line_h * 2.0f},
                             "WASD pan  Q/E or wheel zoom  Space reset  F1/` HUD",
                             midas::Color::bronze());
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

        // Engine first, then GPU textures: C++ destroys in reverse, which is
        // the SDL order (texture → renderer → window → SDL_Quit).
        midas::Engine engine{std::move(config)};

        LoadedSprite sprite = load_sprite(engine, argc > 0 ? argv[0] : nullptr, smoke_ticks > 0);
        if (sprite.from_bmp) {
            std::cerr << "Midas: loaded BMP " << sprite.texture.width() << "x" << sprite.texture.height()
                      << " from " << sprite.path.string() << '\n';
        }

        const midas::Vec2 viewport = engine.renderer().logical_size();
        DemoScene scene = build_demo_scene(sprite.texture, viewport.x, viewport.y);

        midas::Camera camera = engine.renderer().camera();

        // Interactive demos show the HUD (dt/fps/camera plus a one-line
        // WASD/zoom/F1 legend); --smoke asserts math/BMP and never draws it.
        bool show_hud = smoke_ticks == 0;
        if (show_hud) {
            std::cerr << "Midas: F1 or ` toggles the debug HUD\n";
        }

        const int status = engine.run([&](midas::Engine& engine) {
            // Quit before camera/HUD work so the last tick does not present a
            // half-updated frame (close-box already skips on_tick entirely).
            if (engine.input().key_pressed(midas::Key::Escape)) {
                engine.request_quit();
                return;
            }
            // Smoke never enables the overlay (dummy video, no debug text).
            if (smoke_ticks == 0 && (engine.input().key_pressed(midas::Key::F1) ||
                                     engine.input().key_pressed(midas::Key::Grave))) {
                show_hud = !show_hud;
            }

            apply_camera_controls(engine, camera);
            engine.renderer().set_camera(camera);

            // Teaching Color::lerp: the solid plinth eases gold ↔ bronze.
            const float pulse =
                0.5f + 0.5f * std::sin(static_cast<float>(engine.time().elapsed_seconds()) * 1.2f);
            scene.entities[scene.plinth_index].color =
                midas::Color::lerp(midas::Color::gold(), midas::Color::bronze(), pulse);

            auto& renderer = engine.renderer();
            renderer.clear(midas::Color::charcoal());
            for (const auto& entity : scene.entities) {
                midas::draw_entity(renderer, entity);
            }
            if (show_hud) {
                draw_debug_overlay(engine, camera);
            }
            renderer.present();
        });

        if (smoke_ticks > 0) {
            std::cerr << "Midas: smoke completed " << smoke_ticks << " ticks (" << scene.entities.size()
                      << " entities, " << std::fixed << std::setprecision(0)
                      << engine.time().frames_per_second() << " fps, camera/AABB self-check ok)\n";
        }
        return status;
    } catch (const std::exception& ex) {
        std::cerr << "Midas: " << ex.what() << '\n';
        return 1;
    }
}
