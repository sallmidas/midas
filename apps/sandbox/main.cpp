#include <midas/midas.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
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

/// `--smoke` / `MIDAS_SMOKE_FRAMES` count **simulation ticks** (`on_update`),
/// not display presents. A hitch may present once after several ticks.
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

/// Header-only math, before SDL so a broken Camera/Rect cannot hide behind a
/// missing GPU. Covers the constexpr/NaN contracts on clamp, Vec2, Color, Rect,
/// Camera, Transform, Entity (including empty `source` = whole texture and
/// default `TextureId{}` = solid fill), Cooldown,
/// CPU `make_checkerboard_rgba` (including a 2-cell atlas sheet), and the
/// three-space mouse policy. `--smoke` requires the BMP, so the checkerboard
/// fallback would otherwise never run. `SDL_RenderCoordinatesFromWindow` stays
/// interactive-only.
void self_check_math() {
    using midas::Camera;
    using midas::Cooldown;
    using midas::Entity;
    using midas::Rect;
    using midas::Transform;
    using midas::Vec2;
    using midas::clamp;
    using midas::clamp01;

    static_assert(midas::Time::tick_hz == 60);
    static_assert(midas::Engine::max_catch_up == 4);
    static_assert(clamp(0.25f, 0.0f, 1.0f) == 0.25f);
    static_assert(clamp(-2.0f, 0.0f, 1.0f) == 0.0f);
    static_assert(clamp(4.0f, 0.0f, 1.0f) == 1.0f);
    static_assert(clamp01(1.5f) == 1.0f);
    static_assert(clamp(3.0f, 5.0f, 1.0f) == 5.0f);
    static_assert(midas::Rect{0.0f, 0.0f, 10.0f, 10.0f}.contains({0.0f, 0.0f}));
    static_assert(!midas::Rect{0.0f, 0.0f, 10.0f, 10.0f}.contains({10.0f, 10.0f}));
    static_assert(midas::Rect{0.0f, 0.0f, 10.0f, 10.0f}.contains_inclusive({10.0f, 10.0f}));

    if (clamp(0.25f, 0.0f, 1.0f) != 0.25f || clamp(-2.0f, 0.0f, 1.0f) != 0.0f ||
        clamp(4.0f, 0.0f, 1.0f) != 1.0f || clamp01(1.5f) != 1.0f ||
        clamp01(std::numeric_limits<float>::quiet_NaN()) != 0.0f ||
        clamp(3.0f, 5.0f, 1.0f) != 5.0f) {
        throw std::runtime_error("Midas self-check: clamp/clamp01 failed");
    }

    const Vec2 three_four{3.0f, 4.0f};
    const Vec2 unit = three_four.normalized_or_zero();
    const Vec2 nan_dir{std::numeric_limits<float>::quiet_NaN(), 1.0f};
    const Vec2 inf_dir{std::numeric_limits<float>::infinity(), 0.0f};
    const Vec2 zero_dir = Vec2{}.normalized_or_zero();
    if (std::abs(three_four.length_squared() - 25.0f) > 0.001f ||
        std::abs(three_four.length() - 5.0f) > 0.001f ||
        Vec2{}.length_squared() != 0.0f || zero_dir.x != 0.0f || zero_dir.y != 0.0f ||
        std::abs(unit.x - 0.6f) > 0.001f || std::abs(unit.y - 0.8f) > 0.001f ||
        nan_dir.normalized_or_zero().x != 0.0f || nan_dir.normalized_or_zero().y != 0.0f ||
        inf_dir.normalized_or_zero().x != 0.0f || inf_dir.normalized_or_zero().y != 0.0f) {
        throw std::runtime_error("Midas self-check: Vec2 length / normalized_or_zero failed");
    }

    Camera camera;
    camera.position = {kViewportW * 0.5f, kViewportH * 0.5f};
    camera.zoom = 1.0f;

    {
        // Camera{} is zoom 1 at world origin — not the identity logical view.
        Camera raw{};
        const Rect raw_view = raw.visible_world_rect(kViewportW, kViewportH);
        const Rect identity_view = camera.visible_world_rect(kViewportW, kViewportH);
        if (raw.zoom != 1.0f || raw.position.x != 0.0f || raw.position.y != 0.0f ||
            std::abs(identity_view.x) > 0.01f || std::abs(identity_view.y) > 0.01f ||
            std::abs(raw_view.x - identity_view.x) < 1.0f ||
            std::abs(raw_view.y - identity_view.y) < 1.0f) {
            throw std::runtime_error(
                "Midas self-check: Camera{} should not be the identity logical view");
        }
    }

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
    camera.zoom_toward(cursor, std::numeric_limits<float>::quiet_NaN(), kViewportW, kViewportH);
    camera.zoom_toward(cursor, std::numeric_limits<float>::infinity(), kViewportW, kViewportH);
    camera.zoom_toward({std::numeric_limits<float>::quiet_NaN(), cursor.y}, 0.5f, kViewportW, kViewportH);
    camera.zoom_toward(cursor, 0.5f, std::numeric_limits<float>::quiet_NaN(), kViewportH);
    camera.zoom_toward(cursor, 0.5f, kViewportW, 0.0f);
    if (camera.zoom != 2.0f) {
        throw std::runtime_error(
            "Midas self-check: NaN/Inf zoom_toward args should leave zoom unchanged");
    }

    camera.zoom = std::numeric_limits<float>::quiet_NaN();
    if (std::abs(camera.clamped_zoom() - 1.0f) > 0.0f) {
        throw std::runtime_error("Midas self-check: NaN zoom should sanitize to 1");
    }
    camera.zoom = std::numeric_limits<float>::infinity();
    if (std::abs(camera.clamped_zoom() - 1.0f) > 0.0f) {
        throw std::runtime_error("Midas self-check: Inf zoom should sanitize to 1");
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

    // Closed present-rect: far edges are still the view. Half-open `contains`
    // rejects (10, 5) on a 10×10 rect — that is AABB, not letterbox bars.
    const Rect present{0.0f, 0.0f, 1280.0f, 720.0f};
    const Vec2 far_edge{1280.0f, 360.0f};
    const Vec2 far_corner{1280.0f, 720.0f};
    const Vec2 top_bar{640.0f, -40.0f};
    const Vec2 bottom_bar{640.0f, 760.0f};
    const Vec2 left_bar{-1.0f, 360.0f};
    const Vec2 right_bar{1281.0f, 360.0f};
    if (!present.contains({0.0f, 0.0f}) || present.contains(far_edge) || present.contains(far_corner)) {
        throw std::runtime_error("Midas self-check: present-rect half-open contains docs drifted");
    }
    if (!present.contains_inclusive({0.0f, 0.0f}) || !present.contains_inclusive(far_edge) ||
        !present.contains_inclusive(far_corner) || !present.contains_inclusive({640.0f, 720.0f}) ||
        present.contains_inclusive(top_bar) || present.contains_inclusive(bottom_bar) ||
        present.contains_inclusive(left_bar) || present.contains_inclusive(right_bar) ||
        present.contains_inclusive({std::numeric_limits<float>::quiet_NaN(), 360.0f})) {
        throw std::runtime_error("Midas self-check: letterbox present-rect inclusive test failed");
    }

    // Three spaces, no window: 2× framebuffer, matching aspect, EngineConfig
    // logical 1280×720. Window center (640, 360) is logical center; the
    // framebuffer center (1280, 720) is the logical *corner*. Feeding pixels
    // to zoom_toward aims at the wrong world point.
    {
        constexpr float logical_w = 1280.0f;
        constexpr float logical_h = 720.0f;
        Camera cam;
        cam.position = {logical_w * 0.5f, logical_h * 0.5f};
        cam.zoom = 1.0f;
        const Vec2 logical_mouse{logical_w * 0.5f, logical_h * 0.5f};
        const Vec2 pixel_mouse{logical_w, logical_h};
        const Vec2 window_half{logical_w * 0.25f, logical_h * 0.25f};
        const Vec2 world_logical = cam.screen_to_world(logical_mouse, logical_w, logical_h);
        const Vec2 world_pixel = cam.screen_to_world(pixel_mouse, logical_w, logical_h);
        const Vec2 world_window = cam.screen_to_world(window_half, logical_w, logical_h);
        if (std::abs(world_logical.x - cam.position.x) > 0.01f ||
            std::abs(world_logical.y - cam.position.y) > 0.01f ||
            std::abs(world_pixel.x - world_logical.x) < 1.0f ||
            std::abs(world_window.x - world_logical.x) < 1.0f) {
            throw std::runtime_error(
                "Midas self-check: logical/window/pixel mouse spaces should not be interchangeable");
        }
        const Vec2 under = cam.screen_to_world(logical_mouse, logical_w, logical_h);
        cam.zoom_toward(logical_mouse, 2.0f, logical_w, logical_h);
        const Vec2 still = cam.screen_to_world(logical_mouse, logical_w, logical_h);
        if (std::abs(still.x - under.x) > 0.05f || std::abs(still.y - under.y) > 0.05f ||
            std::abs(cam.zoom - 2.0f) > 0.0f) {
            throw std::runtime_error(
                "Midas self-check: zoom_toward in logical space should keep the cursor's world point");
        }
    }

    {
        Cooldown cooldown;
        if (!cooldown.ready() || cooldown.remaining != 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown should start ready");
        }
        cooldown.start(3.0f);
        if (cooldown.ready() || std::abs(cooldown.remaining - 3.0f) > 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown::start should arm remaining");
        }
        cooldown.tick(1.0f);
        cooldown.tick(1.0f);
        if (cooldown.ready() || std::abs(cooldown.remaining - 1.0f) > 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown should need three 1s ticks");
        }
        cooldown.tick(1.0f);
        if (!cooldown.ready() || cooldown.remaining != 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown should expire after its duration");
        }
        cooldown.start(1.0f);
        cooldown.tick(std::numeric_limits<float>::quiet_NaN());
        cooldown.tick(-0.5f);
        if (std::abs(cooldown.remaining - 1.0f) > 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown should ignore non-positive/NaN dt");
        }
        cooldown.start(std::numeric_limits<float>::quiet_NaN());
        if (!cooldown.ready() || cooldown.remaining != 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown::start(NaN) should stay ready");
        }
        cooldown.start(-2.0f);
        if (!cooldown.ready()) {
            throw std::runtime_error("Midas self-check: Cooldown::start of a non-positive duration should stay ready");
        }
        cooldown.start(0.25f);
        cooldown.tick(1.0f);
        if (!cooldown.ready() || cooldown.remaining != 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown should clamp remaining at 0");
        }
        cooldown.remaining = std::numeric_limits<float>::infinity();
        if (!cooldown.ready()) {
            throw std::runtime_error("Midas self-check: Cooldown Inf remaining should not block ready");
        }
        cooldown.tick(1.0f);
        if (!cooldown.ready() || cooldown.remaining != 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown::tick should snap Inf remaining to 0");
        }
        cooldown.remaining = -1.0f;
        if (!cooldown.ready()) {
            throw std::runtime_error("Midas self-check: Cooldown negative remaining should already be ready");
        }
        cooldown.tick(0.1f);
        if (!cooldown.ready() || cooldown.remaining != 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown::tick should snap negative remaining to 0");
        }
        cooldown.remaining = std::numeric_limits<float>::quiet_NaN();
        if (!cooldown.ready()) {
            throw std::runtime_error("Midas self-check: Cooldown NaN remaining should already be ready");
        }
        cooldown.tick(0.1f);
        if (!cooldown.ready() || cooldown.remaining != 0.0f) {
            throw std::runtime_error("Midas self-check: Cooldown::tick should snap NaN remaining to 0");
        }
    }

    const Rect pad{10.0f, 20.0f, 30.0f, 40.0f};
    const Rect grown = pad.expanded(5.0f);
    const Rect shrunk = pad.inset(5.0f);
    const Rect pad_roundtrip = pad.expanded(8.0f).inset(8.0f);
    if (std::abs(grown.x - 5.0f) > 0.0f || std::abs(grown.y - 15.0f) > 0.0f ||
        std::abs(grown.w - 40.0f) > 0.0f || std::abs(grown.h - 50.0f) > 0.0f ||
        std::abs(shrunk.x - 15.0f) > 0.0f || std::abs(shrunk.w - 20.0f) > 0.0f ||
        std::abs(shrunk.h - 30.0f) > 0.0f || pad.inset(5.0f).x != pad.expanded(-5.0f).x ||
        std::abs(pad_roundtrip.x - pad.x) > 0.0f || std::abs(pad_roundtrip.y - pad.y) > 0.0f ||
        std::abs(pad_roundtrip.w - pad.w) > 0.0f || std::abs(pad_roundtrip.h - pad.h) > 0.0f) {
        throw std::runtime_error("Midas self-check: Rect::expanded/inset failed");
    }
    const Rect too_small = pad.inset(20.0f);
    if (too_small.w > 0.0f || too_small.contains({10.0f, 20.0f})) {
        throw std::runtime_error("Midas self-check: heavy Rect::inset should be empty");
    }
    const Rect nan_pad = pad.expanded(std::numeric_limits<float>::quiet_NaN());
    if (nan_pad.x != pad.x || nan_pad.y != pad.y || nan_pad.w != pad.w || nan_pad.h != pad.h) {
        throw std::runtime_error("Midas self-check: NaN Rect::expanded should be a no-op");
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
    const Color clamped_inf = Color::lerp(gold, bronze, std::numeric_limits<float>::infinity());
    if (std::abs(at0.r - gold.r) > 0.001f || std::abs(at0.g - gold.g) > 0.001f ||
        std::abs(at1.r - bronze.r) > 0.001f || std::abs(at1.b - bronze.b) > 0.001f ||
        std::abs(clamped_hi.r - bronze.r) > 0.001f || std::abs(clamped_lo.r - gold.r) > 0.001f ||
        std::abs(clamped_nan.r - gold.r) > 0.001f || std::abs(clamped_inf.r - bronze.r) > 0.001f ||
        std::abs(mid.r - (gold.r + bronze.r) * 0.5f) > 0.001f ||
        std::abs(mid.a - 1.0f) > 0.001f) {
        throw std::runtime_error("Midas self-check: Color::lerp failed");
    }

    // CPU upload path: --smoke requires the BMP, so this would otherwise never run.
    {
        const auto px =
            midas::make_checkerboard_rgba(2, 2, Color::gold(), Color::bronze(), 1);
        if (px.size() != 16) {
            throw std::runtime_error("Midas self-check: checkerboard byte count failed");
        }
        const auto to_u8 = [](float channel) -> std::uint8_t {
            return static_cast<std::uint8_t>(channel * 255.0f + 0.5f);
        };
        // cell_size 1: (0,0) even/gold, (1,0) odd/bronze
        if (px[0] != to_u8(Color::gold().r) || px[4] != to_u8(Color::bronze().r) ||
            px[3] != 255 || px[7] != 255) {
            throw std::runtime_error("Midas self-check: checkerboard pattern / opaque alpha failed");
        }
        Color nan_alpha = Color::white();
        nan_alpha.a = std::numeric_limits<float>::quiet_NaN();
        const auto with_nan_a = midas::make_checkerboard_rgba(1, 1, nan_alpha, nan_alpha, 1);
        if (with_nan_a.size() != 4 || with_nan_a[3] != 255) {
            throw std::runtime_error("Midas self-check: checkerboard NaN alpha should fall back to 1");
        }
        Color nan_rgb{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f};
        const auto with_nan_r = midas::make_checkerboard_rgba(1, 1, nan_rgb, nan_rgb, 1);
        if (with_nan_r[0] != 0) {
            throw std::runtime_error("Midas self-check: checkerboard NaN RGB should fall back to 0");
        }
        Color inf_rgb{std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 1.0f};
        const auto with_inf_r = midas::make_checkerboard_rgba(1, 1, inf_rgb, inf_rgb, 1);
        if (with_inf_r[0] != 0) {
            throw std::runtime_error("Midas self-check: checkerboard Inf RGB should fall back to 0");
        }
        Color inf_alpha = Color::white();
        inf_alpha.a = std::numeric_limits<float>::infinity();
        const auto with_inf_a = midas::make_checkerboard_rgba(1, 1, inf_alpha, inf_alpha, 1);
        if (with_inf_a.size() != 4 || with_inf_a[3] != 255) {
            throw std::runtime_error("Midas self-check: checkerboard Inf alpha should fall back to 1");
        }

        // 2×1 atlas: cell_size == half width → left cell even/gold, right odd/bronze.
        const auto sheet = midas::make_checkerboard_rgba(4, 2, Color::gold(), Color::bronze(), 2);
        if (sheet.size() != 32 || sheet[0] != to_u8(Color::gold().r) ||
            sheet[8] != to_u8(Color::bronze().r)) {
            throw std::runtime_error("Midas self-check: 2-cell atlas checkerboard failed");
        }
    }

    {
        using midas::TextureId;
        const TextureId none{};
        const TextureId minted{0, 1};
        const TextureId stale{99, 1};
        if (none.valid() || static_cast<bool>(none) || !minted.valid() || !stale.valid() ||
            none == minted || minted == stale || !(none == TextureId{})) {
            throw std::runtime_error("Midas self-check: TextureId default / equality failed");
        }
        Entity fill;
        Entity sprite;
        sprite.texture = minted;
        if (fill.texture.valid() || fill.texture != TextureId{} || !sprite.texture.valid()) {
            throw std::runtime_error("Midas self-check: Entity default texture should be invalid");
        }
    }

    {
        Entity whole;
        Entity cell;
        cell.source = {0.0f, 0.0f, 32.0f, 32.0f};
        if (whole.source.w != 0.0f || whole.source.h != 0.0f ||
            !(cell.source.w > 0.0f) || !(cell.source.h > 0.0f)) {
            throw std::runtime_error("Midas self-check: Entity source default / cell rect failed");
        }
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
    midas::TextureId id{};
    bool from_bmp{false};
    std::filesystem::path path;
};

LoadedSprite load_sprite(midas::Engine& engine, const char* argv0, bool smoke) {
    constexpr int kSize = 64;
    auto& renderer = engine.renderer();

    if (const auto path =
            find_asset(engine.executable_directory(), argv0, "midas_sprite.bmp", smoke)) {
        midas::TextureId id = renderer.load_bmp(path->string());
        return LoadedSprite{id, true, *path};
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

midas::Entity make_sprite(midas::Vec2 position, midas::Vec2 size, midas::TextureId texture,
                          midas::Color tint, midas::Rect source = {}) {
    midas::Entity entity;
    entity.transform.position = position;
    entity.size = size;
    entity.texture = texture;
    entity.color = tint;
    entity.source = source;
    return entity;
}

/// Two solid cells side by side (gold | bronze). One CPU buffer, one GPU upload.
midas::TextureId make_atlas(midas::Renderer& renderer) {
    constexpr int cell = 32;
    const auto pixels = midas::make_checkerboard_rgba(
        cell * 2, cell, midas::Color::gold(), midas::Color::bronze(), cell);
    return renderer.create_texture(cell * 2, cell, pixels);
}

struct DemoScene {
    std::vector<midas::Entity> entities;
    std::size_t plinth_index{0};
};

DemoScene build_demo_scene(midas::TextureId sprite, midas::TextureId atlas, float viewport_w,
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

    // Sprite atlas: two source rects from one texture (gold cell | bronze cell).
    constexpr float atlas_cell = 32.0f;
    constexpr float atlas_dest = 64.0f;
    constexpr float atlas_gap = 12.0f;
    const float atlas_y = center.y + 140.0f;
    const float atlas_x = center.x - atlas_dest - atlas_gap * 0.5f;
    scene.push_back(make_sprite({atlas_x, atlas_y}, {atlas_dest, atlas_dest}, atlas,
                                midas::Color::white(), {0.0f, 0.0f, atlas_cell, atlas_cell}));
    scene.push_back(make_sprite({atlas_x + atlas_dest + atlas_gap, atlas_y}, {atlas_dest, atlas_dest},
                                atlas, midas::Color::white(),
                                {atlas_cell, 0.0f, atlas_cell, atlas_cell}));

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
        const midas::Vec2 mouse = input.mouse_position();
        const midas::Rect logical_view{0.0f, 0.0f, viewport_w, viewport_h};
        // SDL maps drawable content onto the closed logical rect
        // `[0, logical_w] × [0, logical_h]`. Half-open `contains` would skip
        // wheel zoom at the right/bottom edges (those are not letterbox bars).
        if (logical_view.contains_inclusive(mouse)) {
            constexpr float wheel_step = 1.15f;
            camera.zoom_toward(mouse, std::pow(wheel_step, input.wheel_y()), viewport_w, viewport_h);
        }
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
        config.width = static_cast<int>(kViewportW);
        config.height = static_cast<int>(kViewportH);
        config.max_ticks = smoke_ticks;

        // Renderer owns GPU textures; TextureId copies are valid until Engine dies.
        midas::Engine engine{std::move(config)};

        LoadedSprite sprite = load_sprite(engine, argc > 0 ? argv[0] : nullptr, smoke_ticks > 0);
        auto& renderer = engine.renderer();
        if (sprite.from_bmp) {
            std::cerr << "Midas: loaded BMP " << renderer.texture_width(sprite.id) << "x"
                      << renderer.texture_height(sprite.id) << " from " << sprite.path.string()
                      << '\n';
        }

        midas::TextureId atlas = make_atlas(renderer);
        std::cerr << "Midas: atlas " << renderer.texture_width(atlas) << "x"
                  << renderer.texture_height(atlas) << " (2 cells, one upload)\n";

        if (smoke_ticks > 0) {
            if (!renderer.texture_valid(sprite.id) || !renderer.texture_valid(atlas) ||
                renderer.texture_width(atlas) != 64 || renderer.texture_height(atlas) != 32 ||
                renderer.texture_width(midas::TextureId{}) != 0 ||
                renderer.texture_valid(midas::TextureId{99, 1})) {
                throw std::runtime_error("Midas self-check: TextureId registry / atlas size failed");
            }
            // Invalid / stale handles skip — they must not throw or crash.
            renderer.draw_texture(midas::TextureId{}, {10.0f, 10.0f, 8.0f, 8.0f});
            renderer.draw_texture(midas::TextureId{99, 1}, {10.0f, 10.0f, 8.0f, 8.0f},
                                  {0.0f, 0.0f, 32.0f, 32.0f});
        }

        const midas::Vec2 viewport = renderer.logical_size();
        DemoScene scene = build_demo_scene(sprite.id, atlas, viewport.x, viewport.y);

        // Identity logical view (center, zoom 1). Camera{} would be origin, not this.
        midas::Camera camera = engine.renderer().camera();

        // Interactive demos show the HUD (dt/fps/camera plus a one-line
        // WASD/zoom/F1 legend); --smoke asserts math/BMP and never draws it.
        bool show_hud = smoke_ticks == 0;
        if (show_hud) {
            std::cerr << "Midas: F1 or ` toggles the debug HUD\n";
            const auto& window = engine.window();
            std::cerr << "Midas: logical " << renderer.logical_width() << "x"
                      << renderer.logical_height() << "  window " << window.width() << "x"
                      << window.height() << "  pixels " << window.pixel_width() << "x"
                      << window.pixel_height() << "  density " << std::fixed << std::setprecision(2)
                      << window.pixel_density() << '\n';
        }

        const int status = engine.run(
            [&](midas::Engine& engine) {
                // Quit before camera/HUD work so the last tick does not present a
                // half-updated frame (close-box already skips update/present).
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

                // Teaching Color::lerp: the solid plinth eases gold ↔ bronze.
                const float pulse =
                    0.5f + 0.5f * std::sin(static_cast<float>(engine.time().elapsed_seconds()) * 1.2f);
                scene.entities[scene.plinth_index].color =
                    midas::Color::lerp(midas::Color::gold(), midas::Color::bronze(), pulse);
            },
            [&](midas::Engine& engine) {
                engine.renderer().set_camera(camera);
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
            std::cerr << "Midas: smoke completed " << smoke_ticks << " simulation ticks ("
                      << scene.entities.size()
                      << " entities, atlas " << engine.renderer().texture_width(atlas) << "x"
                      << engine.renderer().texture_height(atlas)
                      << " 2 cells, math self-check ok)\n";
        }
        return status;
    } catch (const std::exception& ex) {
        std::cerr << "Midas: " << ex.what() << '\n';
        return 1;
    }
}
