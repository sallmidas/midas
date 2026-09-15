# Game v0 — one room

Saul’s victory condition for this slice: **one room, player, walls, key, door, restart. WASD. Fixed room camera. Rects/atlas legal.** Follow-on: **one stationary hazard.** Overlap fails. Avoid it to still win key→door.

## In v0

- `apps/room` (`midas_room`) — a playable binary next to the sandbox.
- Public AABB helpers: `aabb_overlap(Rect, Rect)` and `aabb_move` (axis-separated slide). The room uses both.
- One fixed room. Walls are solid AABBs. The player moves on the floor plane with **WASD** (arrows too) and collides with walls via AABB. Collision stays axis-aligned even if later art looks isometric.
- Key pickup (overlap) unlocks the door. The locked door is a solid; the open door is not. Overlapping the open door wins.
- One stationary hazard (a red AABB on the floor). It is a trigger, not a wall: overlap **fails**, motion freezes, and a fail overlay asks for **R**. The north-corridor key→door path still wins if you never touch it. Win and fail stamps are large screen-space rects (block letters), not just 8×8 debug text, and they stay until **R**.
- **R** restarts (also after a win or fail). **Esc** / close box quits.
- Camera is the identity logical view (no pan/zoom required).
- Drawing: Jim's 192×32 BMP atlas (`apps/room/assets/room_atlas.bmp`) via `TextureId` + source rects — player, wall brick, key, door shut, door open, pit. Solid fills if the BMP is missing or `load_bmp` fails. SDL stays out of public headers.

## Out of v0

Chase AI, HP, a second room, loot, a dungeon generator, click-pathing, audio, rotation, z-sort, PNG/SDL_image, ECS, parenting, an event bus, a job system, Metal/Vulkan, a 3D camera, and an editor. The sandbox remains the tech gym.

## Build / run

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/midas_room
```

Headless: `SDL_VIDEODRIVER=dummy ./build/debug/bin/midas_room --smoke` (AABB + scripted key→door win and hazard fail/reset, then a few dummy-video ticks). `ctest --preset debug` runs sandbox smoke and room smoke.
