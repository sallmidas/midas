#pragma once

#include <midas/Types.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace midas::room {

/// Two fixed boxes. Room A uses the 1280×720 identity view; Room B sits flush
/// east of A's door. Snap the camera by `kRoomBSnapX` when `in_room_b()`.
constexpr float kLogicalW = 1280.0f;
constexpr float kLogicalH = 720.0f;
constexpr float kPlayerSpeed = 240.0f;

constexpr Rect kFloor{176.0f, 96.0f, 928.0f, 528.0f};
constexpr Rect kFloorB{1140.0f, 96.0f, 928.0f, 528.0f};
constexpr float kRoomBSnapX = kFloorB.x - kFloor.x;

/// Room A sliding hazard. One axis (X), fixed 1/60 step, explicit left-edge
/// bounds. Swept body is x [300, 552) × y [214, 246): above the pillar, below
/// the north corridor, west of the shelf. It does not cover the key, the door,
/// or the pit. Spawn column x [200, 240) stays west of the track.
constexpr float kMoverSpeed = 120.0f;
constexpr float kMoverMinX = 300.0f;
constexpr float kMoverMaxX = 520.0f;
constexpr Rect kMoverStart{kMoverMinX, 214.0f, 32.0f, 32.0f};

/// Position, single-axis velocity (px/s), and bounce bounds. Not a solid.
struct Mover {
    Rect body{};
    float velocity{};
    float min_x{};
    float max_x{};

    /// Integrate `dt` seconds on X and reverse at the bounds. Non-positive or
    /// non-finite `dt` is ignored. Callers pass the fixed simulation step.
    void step(float dt) noexcept {
        if (!(dt > 0.0f)) {
            return;
        }
        body.x += velocity * dt;
        if (velocity > 0.0f && body.x >= max_x) {
            body.x = max_x;
            velocity = -velocity;
        } else if (velocity < 0.0f && body.x <= min_x) {
            body.x = min_x;
            velocity = -velocity;
        }
    }
};

struct Room {
    static constexpr std::size_t max_walls = 12;

    Rect player{};
    Rect key{};
    Rect door{};
    Rect hazard{};
    Mover mover{};
    Rect goal{};
    std::array<Rect, max_walls> walls{};
    std::size_t wall_count{};
    bool has_key{false};
    bool won{false};
    bool failed{false};

    /// Player center past A's east wall — the open door is the only gap.
    [[nodiscard]] bool in_room_b() const noexcept {
        return player.x + player.w * 0.5f >= kFloorB.x;
    }

    [[nodiscard]] static Room make() {
        Room room;
        room.player = {200.0f, 540.0f, 40.0f, 40.0f};
        room.key = {920.0f, 140.0f, 24.0f, 24.0f};
        room.door = {1104.0f, 296.0f, 36.0f, 128.0f};
        // Trigger AABB, not a solid: overlap fails. South of the north-corridor
        // key path so WASD-north then east still reaches B.
        room.hazard = {400.0f, 500.0f, 96.0f, 80.0f};
        // Second trigger, also not a solid. Starts at the west bound moving east.
        room.mover.body = kMoverStart;
        room.mover.velocity = kMoverSpeed;
        room.mover.min_x = kMoverMinX;
        room.mover.max_x = kMoverMaxX;
        // Room B exit, same door-slot Y as A so a straight east walk wins.
        room.goal = {2068.0f, 296.0f, 36.0f, 128.0f};

        auto add_wall = [&](Rect wall) {
            room.walls[room.wall_count++] = wall;
        };
        add_wall({140.0f, 60.0f, 1000.0f, 36.0f});    // A north
        add_wall({140.0f, 624.0f, 1000.0f, 36.0f});   // A south
        add_wall({140.0f, 96.0f, 36.0f, 528.0f});     // A west
        add_wall({1104.0f, 96.0f, 36.0f, 200.0f});    // A east, above the door
        add_wall({1104.0f, 424.0f, 36.0f, 200.0f});   // A east, below the door
        add_wall({380.0f, 250.0f, 48.0f, 220.0f});    // A pillar
        add_wall({560.0f, 170.0f, 280.0f, 40.0f});    // A shelf
        add_wall({680.0f, 430.0f, 52.0f, 150.0f});    // A stub
        add_wall({1140.0f, 60.0f, 964.0f, 36.0f});    // B north
        add_wall({1140.0f, 624.0f, 964.0f, 36.0f});   // B south
        add_wall({2068.0f, 96.0f, 36.0f, 200.0f});    // B east, above the goal
        add_wall({2068.0f, 424.0f, 36.0f, 200.0f});   // B east, below the goal
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

    /// `wish` is a WASD direction (not required to be unit length). `dt` is the
    /// fixed simulation step (`Time::delta_seconds`, 1/60), not a display-frame
    /// delta — the mover integrates with that same step. `restart` reloads
    /// Room A spawn, including mover position and direction. After a win or
    /// fail, player and mover both freeze until restart.
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
        mover.step(dt);

        if (aabb_overlap(player, hazard) || aabb_overlap(player, mover.body)) {
            failed = true;
            return;
        }
        if (!has_key && aabb_overlap(player, key)) {
            has_key = true;
        }
        // A's open door is a passage into B, not a win. Win is the goal in B.
        if (in_room_b() && aabb_overlap(player, goal)) {
            won = true;
        }
    }
};

}  // namespace midas::room
