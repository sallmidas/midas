#include "room.hpp"

#include <midas/midas.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kSmokeTicks = 3;
constexpr int kAtlasCell = 32;
constexpr int kAtlasCols = 6;
constexpr midas::Color kHazardFill{0.78f, 0.16f, 0.12f, 1.0f};
constexpr midas::Color kPlayerFill = midas::Color::gold();
constexpr midas::Color kKeyFill{0.95f, 0.88f, 0.40f, 1.0f};

constexpr midas::Rect atlas_cell(int index) noexcept {
    return {static_cast<float>(kAtlasCell * index), 0.0f, static_cast<float>(kAtlasCell),
            static_cast<float>(kAtlasCell)};
}

// Jim's 192×32 sheet, left → right: player, wall, key, door shut, door open, pit.
constexpr midas::Rect kPlayerCell = atlas_cell(0);
constexpr midas::Rect kWallCell = atlas_cell(1);
constexpr midas::Rect kKeyCell = atlas_cell(2);
constexpr midas::Rect kDoorShutCell = atlas_cell(3);
constexpr midas::Rect kDoorOpenCell = atlas_cell(4);
constexpr midas::Rect kPitCell = atlas_cell(5);

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

void add_candidate(std::vector<std::filesystem::path>& candidates, std::filesystem::path path) {
    if (path.empty()) {
        return;
    }
    candidates.push_back(std::move(path));
}

std::optional<std::filesystem::path> find_room_atlas(const std::filesystem::path& exe_dir,
                                                     const char* argv0, bool next_to_binary_only) {
    std::vector<std::filesystem::path> candidates;
    add_candidate(candidates, exe_dir / "assets" / "room_atlas.bmp");

    if (argv0 != nullptr && argv0[0] != '\0') {
        add_candidate(candidates, std::filesystem::path(argv0).parent_path() / "assets" /
                                      "room_atlas.bmp");
    }

    if (!next_to_binary_only) {
        std::error_code cwd_ec;
        const auto cwd = std::filesystem::current_path(cwd_ec);
        if (!cwd_ec) {
            add_candidate(candidates, cwd / "assets" / "room_atlas.bmp");
            add_candidate(candidates, cwd / "apps" / "room" / "assets" / "room_atlas.bmp");
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

struct LoadedAtlas {
    midas::TextureId id{};
    bool from_bmp{false};
};

/// Jim's sheet via `load_bmp`. Missing or unloadable BMP → invalid id; draws
/// stay solid rects (Saul). `--smoke` only looks next to the binary so a
/// broken CMake copy is visible, but it still must not fail the run.
LoadedAtlas load_room_atlas(midas::Engine& engine, const char* argv0, bool smoke) {
    auto& renderer = engine.renderer();
    const auto path = find_room_atlas(engine.executable_directory(), argv0, smoke);
    if (!path) {
        std::cerr << "Midas room: missing assets/room_atlas.bmp; using solid rects\n"
                  << "  looked in " << (engine.executable_directory() / "assets").string();
        if (!smoke) {
            std::cerr << ", argv0, ./assets, and ./apps/room/assets";
        }
        std::cerr << '\n';
        return {};
    }

    try {
        const midas::TextureId id = renderer.load_bmp(path->string());
        const int width = renderer.texture_width(id);
        const int height = renderer.texture_height(id);
        if (!renderer.texture_valid(id) || width != kAtlasCell * kAtlasCols ||
            height != kAtlasCell) {
            std::cerr << "Midas room: " << path->string() << " is " << width << "x" << height
                      << " (expected " << (kAtlasCell * kAtlasCols) << "x" << kAtlasCell
                      << "); using solid rects\n";
            return {};
        }
        return {id, true};
    } catch (const std::exception& ex) {
        std::cerr << "Midas room: failed to load " << path->string() << " (" << ex.what()
                  << "); using solid rects\n";
        return {};
    }
}

void draw_sprite_or_fill(midas::Renderer& renderer, midas::TextureId atlas, bool use_atlas,
                         const midas::Rect& dest, const midas::Rect& src,
                         const midas::Color& fill) {
    if (use_atlas) {
        renderer.draw_texture(atlas, dest, src);
    } else {
        renderer.fill_rect(dest, fill);
    }
}

/// Repeat a 32×32 brick across a wall AABB. Partial edge tiles clip `src`.
void draw_tiles_or_fill(midas::Renderer& renderer, midas::TextureId atlas, bool use_atlas,
                        const midas::Rect& dest, const midas::Rect& src,
                        const midas::Color& fill) {
    if (!use_atlas) {
        renderer.fill_rect(dest, fill);
        return;
    }
    if (src.w <= 0.0f || src.h <= 0.0f || dest.w <= 0.0f || dest.h <= 0.0f) {
        return;
    }
    for (float y = 0.0f; y < dest.h;) {
        const float h = std::min(src.h, dest.h - y);
        for (float x = 0.0f; x < dest.w;) {
            const float w = std::min(src.w, dest.w - x);
            renderer.draw_texture(atlas, midas::Rect{dest.x + x, dest.y + y, w, h},
                                  midas::Rect{src.x, src.y, w, h});
            x += w;
        }
        y += h;
    }
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

/// 5×7 bitmap (`#` = fill). Used so WIN/FAIL are readable as shapes without a
/// font atlas — `draw_debug_text` is an 8×8 SDL bitmap and easy to miss live.
void draw_block_glyph(midas::Renderer& renderer, float ox, float oy, float cell,
                      std::string_view bits, const midas::Color& color) {
    constexpr int cols = 5;
    constexpr int rows = 7;
    if (bits.size() < static_cast<std::size_t>(cols * rows)) {
        return;
    }
    const float pad = cell * 0.12f;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (bits[static_cast<std::size_t>(row * cols + col)] != '#') {
                continue;
            }
            renderer.fill_screen_rect(
                {ox + static_cast<float>(col) * cell + pad,
                 oy + static_cast<float>(row) * cell + pad, cell - pad * 2.0f,
                 cell - pad * 2.0f},
                color);
        }
    }
}

const char* glyph_bits(char letter) {
    switch (letter) {
        case 'W':
            // Join sits low so this reads as W, not M.
            return "#   ##   ##   ## # ## # ### ###   #";
        case 'I':
            return "#####  #    #    #    #    #  #####";
        case 'N':
            return "#   ###  ## # ## # ## # ##  ###   #";
        case 'F':
            return "######    #    #### #    #    #    ";
        case 'A':
            return " ### #   ##   #######   ##   ##   #";
        case 'L':
            return "#    #    #    #    #    #    #####";
        default:
            return nullptr;
    }
}

void draw_block_word(midas::Renderer& renderer, float x, float y, float cell,
                     std::string_view word, const midas::Color& color) {
    const float advance = cell * 5.0f + cell * 0.7f;
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (const char* bits = glyph_bits(word[i])) {
            draw_block_glyph(renderer, x + static_cast<float>(i) * advance, y, cell, bits,
                             color);
        }
    }
}

/// Full-present stamp. Drawn every frame while `won` / `failed` so it stays
/// until R. Block letters + bars — not the 8×8 debug bitmap.
void draw_end_overlay(midas::Renderer& renderer, bool won) {
    const midas::Color accent = won ? midas::Color::gold() : kHazardFill;
    const midas::Color wash = won ? midas::Color{0.831f, 0.686f, 0.216f, 0.72f}
                                  : midas::Color{0.78f, 0.16f, 0.12f, 0.72f};

    renderer.fill_screen_rect({0.0f, 0.0f, midas::room::kLogicalW, midas::room::kLogicalH}, wash);

    const midas::Rect frame{48.0f, 48.0f, midas::room::kLogicalW - 96.0f,
                            midas::room::kLogicalH - 96.0f};
    renderer.fill_screen_rect(frame, accent);
    renderer.fill_screen_rect(frame.inset(22.0f), midas::Color::charcoal());

    renderer.fill_screen_rect({48.0f, 48.0f, midas::room::kLogicalW - 96.0f, 36.0f}, accent);
    renderer.fill_screen_rect({48.0f, midas::room::kLogicalH - 84.0f, midas::room::kLogicalW - 96.0f, 36.0f},
                              accent);

    const char* word = won ? "WIN" : "FAIL";
    const std::size_t letters = won ? 3 : 4;
    constexpr float cell = 36.0f;
    const float word_w =
        static_cast<float>(letters) * cell * 5.0f + static_cast<float>(letters - 1) * cell * 0.7f;
    const float word_h = cell * 7.0f;
    const float word_x = (midas::room::kLogicalW - word_w) * 0.5f;
    const float word_y = 210.0f;
    draw_block_word(renderer, word_x, word_y, cell, word, accent);

    // Small "R" cue as bars (debug text is too small to trust live).
    const midas::Rect restart_bar{word_x, word_y + word_h + 36.0f, word_w, 28.0f};
    renderer.fill_screen_rect(restart_bar, accent);
    renderer.fill_screen_rect(restart_bar.inset(6.0f), midas::Color::charcoal());
    renderer.draw_debug_text({word_x + 8.0f, restart_bar.y + 8.0f}, "R RESTART",
                             midas::Color::white());
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
        const LoadedAtlas atlas = load_room_atlas(engine, argc > 0 ? argv[0] : nullptr,
                                                  smoke_ticks > 0);
        const bool use_atlas = atlas.from_bmp && renderer.texture_valid(atlas.id);

        midas::room::Room room = midas::room::Room::make();
        midas::Camera camera = renderer.camera();
        const bool show_hud = smoke_ticks == 0;

        if (show_hud) {
            std::cerr << "Midas room: WASD move, avoid the red pit, get the key, walk through "
                         "the door. R restarts.\n";
            if (use_atlas) {
                std::cerr << "Midas room: drawing Jim's room_atlas.bmp cells\n";
            }
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
                    draw_tiles_or_fill(renderer, atlas.id, use_atlas, room.walls[i], kWallCell,
                                       midas::Color::bronze());
                }

                draw_sprite_or_fill(renderer, atlas.id, use_atlas, room.hazard, kPitCell,
                                    kHazardFill);

                const bool door_open = room.has_key || room.won;
                draw_sprite_or_fill(renderer, atlas.id, use_atlas, room.door,
                                    door_open ? kDoorOpenCell : kDoorShutCell,
                                    door_open ? midas::Color::gold() : midas::Color::bronze());
                if (!room.has_key) {
                    draw_sprite_or_fill(renderer, atlas.id, use_atlas, room.key, kKeyCell,
                                        kKeyFill);
                }
                draw_sprite_or_fill(renderer, atlas.id, use_atlas, room.player, kPlayerCell,
                                    kPlayerFill);

                if (show_hud) {
                    draw_hud(renderer, room);
                }
                // Overlay is not HUD-gated: it must stay on screen every present
                // after win/fail until R (tiny debug text alone is easy to miss).
                if (room.won || room.failed) {
                    draw_end_overlay(renderer, room.won);
                }
                renderer.present();
            });

        if (smoke_ticks > 0) {
            std::cerr << "Midas room: smoke completed " << smoke_ticks
                      << " simulation ticks (aabb + room self-check ok, "
                      << (use_atlas ? "atlas loaded" : "solid-rect fallback") << ")\n";
        }
        return status;
    } catch (const std::exception& ex) {
        std::cerr << "Midas room: " << ex.what() << '\n';
        return 1;
    }
}
