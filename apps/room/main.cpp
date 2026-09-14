#include "room.hpp"

#include <midas/midas.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kSmokeTicks = 3;
constexpr int kAtlasCell = 32;
constexpr midas::Color kHazardFill{0.78f, 0.16f, 0.12f, 1.0f};

int parse_smoke_ticks(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--smoke") {
            return kSmokeTicks;
        }
    }

    if (const char* env = std::getenv("MIDAS_SMOKE_FRAMES")) {
        try {
            const int frames = std::stoi(env);
            if (frames > 0) {
                return frames;
            }
        } catch (const std::exception&) {
            std::cerr << "Midas room: ignoring invalid MIDAS_SMOKE_FRAMES\n";
        }
    }

    return 0;
}

void expect(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

void self_check_aabb() {
    using midas::Rect;
    using midas::Vec2;
    using midas::aabb_move;
    using midas::aabb_overlap;

    static_assert(aabb_overlap(Rect{0.0f, 0.0f, 10.0f, 10.0f}, Rect{5.0f, 5.0f, 10.0f, 10.0f}));
    static_assert(!aabb_overlap(Rect{0.0f, 0.0f, 10.0f, 10.0f}, Rect{10.0f, 0.0f, 10.0f, 10.0f}));

    const Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    const Rect b{5.0f, 5.0f, 10.0f, 10.0f};
    const Rect edge{10.0f, 0.0f, 10.0f, 10.0f};
    const Rect empty{0.0f, 0.0f, 0.0f, 0.0f};
    expect(aabb_overlap(a, b) && !aabb_overlap(a, edge) && !aabb_overlap(a, empty),
           "Midas room self-check: aabb_overlap failed");
    expect(aabb_overlap(a, b) == a.overlaps(b),
           "Midas room self-check: aabb_overlap should match Rect::overlaps");

    const Rect wall{20.0f, 0.0f, 10.0f, 10.0f};
    const Rect body{0.0f, 0.0f, 10.0f, 10.0f};
    const Rect free = aabb_move(body, {4.0f, 3.0f}, {});
    expect(std::abs(free.x - 4.0f) < 0.001f && std::abs(free.y - 3.0f) < 0.001f,
           "Midas room self-check: aabb_move with no solids should apply the delta");

    const Rect blocked_x = aabb_move(body, {15.0f, 0.0f}, std::span<const Rect>(&wall, 1));
    expect(std::abs(blocked_x.x - body.x) < 0.001f && std::abs(blocked_x.y - body.y) < 0.001f,
           "Midas room self-check: aabb_move should reject an overlapping X step");

    // Touching is legal (half-open). Slide along the wall on Y while X is blocked.
    const Rect touch{10.0f, 0.0f, 10.0f, 10.0f};
    const Rect slide = aabb_move(body, {8.0f, 4.0f}, std::span<const Rect>(&touch, 1));
    expect(std::abs(slide.x - body.x) < 0.001f && std::abs(slide.y - 4.0f) < 0.001f,
           "Midas room self-check: aabb_move should slide on the free axis");

    const Rect nan_delta =
        aabb_move(body, {std::numeric_limits<float>::quiet_NaN(), 2.0f}, {});
    expect(std::abs(nan_delta.x - body.x) < 0.001f && std::abs(nan_delta.y - 2.0f) < 0.001f,
           "Midas room self-check: aabb_move should ignore a NaN delta axis");
}

void self_check_room() {
    using midas::room::Room;

    const float dt = static_cast<float>(midas::Time::tick_seconds);

    {
        Room room = Room::make();
        expect(room.wall_count == 8 && !room.has_key && !room.won && !room.failed,
               "Midas room self-check: fresh room should be locked, 8 walls");
        expect(!room.player_overlaps_solid(),
               "Midas room self-check: spawn should not overlap a solid");
        expect(!midas::aabb_overlap(room.player, room.key),
               "Midas room self-check: spawn should not already hold the key");
        expect(!midas::aabb_overlap(room.player, room.door),
               "Midas room self-check: spawn should not overlap the door");
        expect(!midas::aabb_overlap(room.player, room.hazard),
               "Midas room self-check: spawn should not overlap the hazard");
    }

    {
        Room room = Room::make();
        for (int i = 0; i < 180; ++i) {
            room.tick({-1.0f, 0.0f}, dt, false);
        }
        expect(!room.player_overlaps_solid(),
               "Midas room self-check: walking into the west wall should not penetrate");
        expect(!room.failed,
               "Midas room self-check: west-wall walk should not touch the hazard");
        expect(room.player.x + 0.01f >= midas::room::kFloor.x,
               "Midas room self-check: player should stay on the floor side of the west wall");
    }

    {
        Room room = Room::make();
        room.player.x = room.door.x - room.player.w - 4.0f;
        room.player.y = room.door.y + 20.0f;
        for (int i = 0; i < 90; ++i) {
            room.tick({1.0f, 0.0f}, dt, false);
        }
        expect(!room.won && !room.has_key && !room.failed,
               "Midas room self-check: a locked door should not be a win");
        expect(!midas::aabb_overlap(room.player, room.door),
               "Midas room self-check: locked door should block like a wall");
    }

    {
        Room room = Room::make();
        // North corridor to the key, then down and into the open door.
        for (int i = 0; i < 200 && room.player.y > 110.0f; ++i) {
            room.tick({0.0f, -1.0f}, dt, false);
        }
        for (int i = 0; i < 400 && !room.has_key; ++i) {
            room.tick({1.0f, 0.0f}, dt, false);
        }
        expect(room.has_key, "Midas room self-check: scripted path should pick up the key");
        expect(!room.failed,
               "Midas room self-check: key pickup path should not touch the hazard");
        expect(!room.player_overlaps_solid(),
               "Midas room self-check: key pickup path should stay out of walls");

        for (int i = 0; i < 200 && room.player.y + room.player.h < room.door.y + 40.0f; ++i) {
            room.tick({0.0f, 1.0f}, dt, false);
        }
        for (int i = 0; i < 200 && !room.won; ++i) {
            room.tick({1.0f, 0.0f}, dt, false);
        }
        expect(room.won && !room.failed,
               "Midas room self-check: overlapping the open door should win");
        expect(midas::aabb_overlap(room.player, room.door),
               "Midas room self-check: win pose should overlap the open door");

        room.tick({}, dt, true);
        expect(!room.won && !room.failed && !room.has_key &&
                   std::abs(room.player.x - 200.0f) < 0.01f,
               "Midas room self-check: R/restart should restore spawn state");
    }

    {
        Room room = Room::make();
        for (int i = 0; i < 180 && !room.failed; ++i) {
            room.tick({1.0f, 0.0f}, dt, false);
        }
        expect(room.failed && !room.won,
               "Midas room self-check: walking east from spawn should hit the hazard");
        expect(midas::aabb_overlap(room.player, room.hazard),
               "Midas room self-check: fail pose should overlap the hazard");

        const float x_at_fail = room.player.x;
        room.tick({1.0f, 0.0f}, dt, false);
        expect(std::abs(room.player.x - x_at_fail) < 0.01f,
               "Midas room self-check: fail should freeze motion until restart");

        room.tick({}, dt, true);
        expect(!room.failed && !room.won && std::abs(room.player.x - 200.0f) < 0.01f,
               "Midas room self-check: R/restart after fail should restore spawn");
    }
}

midas::TextureId make_actor_atlas(midas::Renderer& renderer) {
    constexpr int cols = 3;
    const int width = kAtlasCell * cols;
    const midas::Color colors[cols] = {
        midas::Color::gold(),
        {0.95f, 0.88f, 0.40f, 1.0f},
        midas::Color::bronze(),
    };

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * kAtlasCell * 4));
    const auto to_u8 = [](float channel) -> std::uint8_t {
        return static_cast<std::uint8_t>(midas::clamp01(channel) * 255.0f + 0.5f);
    };
    for (int cell = 0; cell < cols; ++cell) {
        const midas::Color color = colors[cell];
        for (int y = 0; y < kAtlasCell; ++y) {
            for (int x = 0; x < kAtlasCell; ++x) {
                const std::size_t i =
                    static_cast<std::size_t>((y * width) + (cell * kAtlasCell + x)) * 4;
                pixels[i + 0] = to_u8(color.r);
                pixels[i + 1] = to_u8(color.g);
                pixels[i + 2] = to_u8(color.b);
                pixels[i + 3] = to_u8(color.a);
            }
        }
    }
    return renderer.create_texture(width, kAtlasCell, pixels);
}

midas::Vec2 wish_from_input(const midas::Input& input) {
    midas::Vec2 wish{};
    if (input.key_down(midas::Key::A) || input.key_down(midas::Key::Left)) {
        wish.x -= 1.0f;
    }
    if (input.key_down(midas::Key::D) || input.key_down(midas::Key::Right)) {
        wish.x += 1.0f;
    }
    if (input.key_down(midas::Key::W) || input.key_down(midas::Key::Up)) {
        wish.y -= 1.0f;
    }
    if (input.key_down(midas::Key::S) || input.key_down(midas::Key::Down)) {
        wish.y += 1.0f;
    }
    return wish;
}

void draw_hud(midas::Renderer& renderer, const midas::room::Room& room) {
    constexpr float x = 16.0f;
    constexpr float y = 12.0f;
    constexpr float line_h = 12.0f;
    const midas::Rect banner{x, y, 620.0f, 40.0f};
    renderer.fill_screen_rect(banner.expanded(6.0f), {0.0f, 0.0f, 0.0f, 0.55f});
    renderer.draw_debug_text({x, y}, "WASD move   R restart   Esc quit", midas::Color::gold());

    std::ostringstream status;
    status << (room.has_key ? "Key: GOT IT" : "Key: --") << "   "
           << (room.failed ? "FAIL"
                           : (room.won ? "Door: YOU WIN"
                                       : (room.has_key ? "Door: OPEN" : "Door: locked")));
    renderer.draw_debug_text({x, y + line_h}, status.str(), midas::Color::white());
    renderer.draw_debug_text({x, y + line_h * 2.0f},
                             "Fixed room camera. Avoid the red pit. Overlap the open door to win.",
                             midas::Color::bronze());
}

void draw_end_overlay(midas::Renderer& renderer, bool won) {
    renderer.fill_screen_rect({0.0f, 0.0f, midas::room::kLogicalW, midas::room::kLogicalH},
                              {0.0f, 0.0f, 0.0f, 0.55f});
    const midas::Rect banner{340.0f, 250.0f, 600.0f, 180.0f};
    const midas::Color accent = won ? midas::Color::gold() : kHazardFill;
    renderer.fill_screen_rect(banner, accent);
    renderer.fill_screen_rect(banner.inset(10.0f), midas::Color::charcoal());
    renderer.draw_debug_text({won ? 560.0f : 540.0f, 310.0f}, won ? "YOU WIN" : "YOU FAILED",
                             accent);
    renderer.draw_debug_text({500.0f, 350.0f}, "Press R to restart", midas::Color::white());
}

}  // namespace

int main(int argc, char** argv) {
    try {
        self_check_aabb();
        self_check_room();

        const int smoke_ticks = parse_smoke_ticks(argc, argv);

        midas::EngineConfig config;
        config.title = "Midas room";
        config.width = static_cast<int>(midas::room::kLogicalW);
        config.height = static_cast<int>(midas::room::kLogicalH);
        config.max_ticks = smoke_ticks;

        midas::Engine engine{std::move(config)};
        auto& renderer = engine.renderer();
        const midas::TextureId atlas = make_actor_atlas(renderer);
        if (!renderer.texture_valid(atlas) || renderer.texture_width(atlas) != kAtlasCell * 3 ||
            renderer.texture_height(atlas) != kAtlasCell) {
            throw std::runtime_error("Midas room: actor atlas upload failed");
        }

        midas::room::Room room = midas::room::Room::make();
        midas::Camera camera = renderer.camera();
        const bool show_hud = smoke_ticks == 0;
        constexpr midas::Rect kPlayerCell{0.0f, 0.0f, static_cast<float>(kAtlasCell),
                                          static_cast<float>(kAtlasCell)};
        constexpr midas::Rect kKeyCell{static_cast<float>(kAtlasCell), 0.0f,
                                       static_cast<float>(kAtlasCell),
                                       static_cast<float>(kAtlasCell)};
        constexpr midas::Rect kDoorCell{static_cast<float>(kAtlasCell * 2), 0.0f,
                                        static_cast<float>(kAtlasCell),
                                        static_cast<float>(kAtlasCell)};

        if (show_hud) {
            std::cerr << "Midas room: WASD move, avoid the red pit, get the key, walk through "
                         "the door. R restarts.\n";
        }

        const int status = engine.run(
            [&](midas::Engine& engine) {
                if (engine.input().key_pressed(midas::Key::Escape)) {
                    engine.request_quit();
                    return;
                }

                midas::Vec2 wish{};
                if (engine.input().window_focused() && !room.won && !room.failed) {
                    wish = wish_from_input(engine.input());
                }
                const float dt = static_cast<float>(engine.time().delta_seconds());
                room.tick(wish, dt, engine.input().key_pressed(midas::Key::R));
            },
            [&](midas::Engine& engine) {
                auto& renderer = engine.renderer();
                renderer.set_camera(camera);
                renderer.clear(midas::Color::charcoal());
                renderer.fill_rect(midas::room::kFloor, {0.16f, 0.13f, 0.10f, 1.0f});

                for (std::size_t i = 0; i < room.wall_count; ++i) {
                    renderer.fill_rect(room.walls[i], midas::Color::bronze());
                }

                renderer.fill_rect(room.hazard, kHazardFill);

                const midas::Color door_tint =
                    (room.has_key || room.won) ? midas::Color::gold() : midas::Color::bronze();
                renderer.draw_texture(atlas, room.door, kDoorCell, door_tint);
                if (!room.has_key) {
                    renderer.draw_texture(atlas, room.key, kKeyCell);
                }
                renderer.draw_texture(atlas, room.player, kPlayerCell);

                if (show_hud) {
                    draw_hud(renderer, room);
                    if (room.won || room.failed) {
                        draw_end_overlay(renderer, room.won);
                    }
                }
                renderer.present();
            });

        if (smoke_ticks > 0) {
            std::cerr << "Midas room: smoke completed " << smoke_ticks
                      << " simulation ticks (aabb + room self-check ok)\n";
        }
        return status;
    } catch (const std::exception& ex) {
        std::cerr << "Midas room: " << ex.what() << '\n';
        return 1;
    }
}
