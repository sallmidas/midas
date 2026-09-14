#pragma once

#include <midas/Types.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace midas::room {

/// One fixed dungeon room (v0). World units match the 1280×720 logical present.
constexpr float kLogicalW = 1280.0f;
constexpr float kLogicalH = 720.0f;
constexpr float kPlayerSpeed = 240.0f;

constexpr Rect kFloor{176.0f, 96.0f, 928.0f, 528.0f};

struct Room {
    static constexpr std::size_t max_walls = 8;

    Rect player{};
    Rect key{};
    Rect door{};
    Rect hazard{};
    std::array<Rect, max_walls> walls{};
    std::size_t wall_count{};
    bool has_key{false};
    bool won{false};
    bool failed{false};

    [[nodiscard]] static Room make() {
        Room room;
        room.player = {200.0f, 540.0f, 40.0f, 40.0f};
        room.key = {920.0f, 140.0f, 24.0f, 24.0f};
        room.door = {1104.0f, 296.0f, 36.0f, 128.0f};
        // Trigger AABB, not a solid: overlap fails. South of the north-corridor
        // key path so WASD-north then east still wins.
        room.hazard = {400.0f, 500.0f, 96.0f, 80.0f};

        auto add_wall = [&](Rect wall) {
            room.walls[room.wall_count++] = wall;
        };
        add_wall({140.0f, 60.0f, 1000.0f, 36.0f});    // north
        add_wall({140.0f, 624.0f, 1000.0f, 36.0f});   // south
        add_wall({140.0f, 96.0f, 36.0f, 528.0f});     // west
        add_wall({1104.0f, 96.0f, 36.0f, 200.0f});    // east, above the door
        add_wall({1104.0f, 424.0f, 36.0f, 200.0f});   // east, below the door
        add_wall({380.0f, 250.0f, 48.0f, 220.0f});    // pillar
        add_wall({560.0f, 170.0f, 280.0f, 40.0f});    // shelf
        add_wall({680.0f, 430.0f, 52.0f, 150.0f});    // stub
        return room;
    }

    void reset() { *this = make(); }

    /// Solids the player cannot walk through: walls, plus the door while locked.
    [[nodiscard]] std::size_t copy_solids(std::span<Rect> out) const {
        std::size_t n = 0;
        for (std::size_t i = 0; i < wall_count && n < out.size(); ++i) {
            out[n++] = walls[i];
        }
        if (!has_key && n < out.size()) {
            out[n++] = door;
        }
        return n;
    }

    [[nodiscard]] bool player_overlaps_solid() const {
        std::array<Rect, max_walls + 1> solids{};
        const std::size_t n = copy_solids(solids);
        for (std::size_t i = 0; i < n; ++i) {
            if (aabb_overlap(player, solids[i])) {
                return true;
            }
        }
        return false;
    }

    /// `wish` is a WASD direction (not required to be unit length). `restart`
    /// reloads the room. After a win or fail, motion freezes until restart.
    void tick(Vec2 wish, float dt, bool restart) {
        if (restart) {
            reset();
            return;
        }
        if (won || failed) {
            return;
        }

        std::array<Rect, max_walls + 1> solids{};
        const std::size_t n = copy_solids(solids);
        const Vec2 delta = wish.normalized_or_zero() * kPlayerSpeed * dt;
        player = aabb_move(player, delta, std::span<const Rect>(solids.data(), n));

        if (aabb_overlap(player, hazard)) {
            failed = true;
            return;
        }
        if (!has_key && aabb_overlap(player, key)) {
            has_key = true;
        }
        if (has_key && aabb_overlap(player, door)) {
            won = true;
        }
    }
};

}  // namespace midas::room
