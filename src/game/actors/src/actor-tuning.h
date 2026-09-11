#pragma once

/// @file actor-tuning.h
/// @brief The constants actor movement and planning are tuned by.
/// @par Threading
/// Constants.

#include <cstdint>

namespace eng::game {

/// How near its goal an actor walking to a place, rather than a target,
/// counts as arrived, in tiles.
inline constexpr float ACTOR_ARRIVE_TILES = 0.2F;

/// How near a waypoint an actor counts as having reached it and moves on
/// to the next, in tiles. More than the fastest step, so no actor steps
/// over a waypoint and turns back for it.
inline constexpr float ACTOR_WAYPOINT_TILES = 0.35F;

/// How many cells out a goal an actor cannot stand on — beside a crate,
/// inside one — looks for the nearest spot it can: two tiles.
inline constexpr uint32_t ACTOR_SNAP_RINGS = 8;

/// Ticks between replans of a path whose goal has moved: a quarter second.
/// A pursuer's goal moves with its quarry every few ticks; replanning each
/// time would be wasted, and a straight walk takes over anyway once the
/// quarry is in view.
inline constexpr uint64_t ACTOR_REPLAN_TICKS = 15;

/// How near a player an actor is in the near tier, in tiles: about as far
/// as the camera shows. A near actor perceives every tick.
inline constexpr float ACTOR_NEAR_TIER_TILES = 16.0F;

/// How often an actor further from every player than the near tier
/// perceives, in ticks: a tenth of a second, which nobody sees from off
/// the screen, and a sixth of the line-of-sight walks.
inline constexpr uint64_t ACTOR_FAR_PERCEIVE_TICKS = 6;

/// How far past its radius an actor moving looks for boxes it could be
/// pushed into, in tiles. A tick's step and the push out of a prop are
/// both far shorter, so the boxes gathered are every box the full list
/// would have stopped it against.
inline constexpr float ACTOR_COLLISION_REACH_TILES = 1.0F;

/// How near a player it perceives a pursuer walks straight at them, when
/// nothing is in the way, rather than down their flow field, in tiles.
/// Further out it keeps to the field, which costs no line-of-sight walk.
inline constexpr float ACTOR_FLOW_DIRECT_TILES = 6.0F;

/// How many cells down a flow field a pursuer aims, each tick: a tile,
/// further than `ACTOR_WAYPOINT_TILES`, so the point is never already
/// reached.
inline constexpr uint32_t ACTOR_FLOW_LOOKAHEAD_CELLS = 4;

/// The smallest displacement that counts as moving, in tiles.
inline constexpr float ACTOR_MIN_STEP_TILES = 0.002F;

/// The fraction of an intended step an actor must make to count as not
/// blocked. Sliding along a wall keeps more than this; walking into one
/// head-on does not.
inline constexpr float ACTOR_BLOCKED_FRACTION = 0.25F;

/// The chance per tick, in thousandths, that a wanderer standing at its
/// spot picks another: a pause of a second or so on average.
inline constexpr uint32_t ACTOR_WANDER_REPICK_PERMILLE = 15;

}  // namespace eng::game
