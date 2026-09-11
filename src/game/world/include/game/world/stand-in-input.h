#pragma once

/// @file stand-in-input.h
/// @brief Input for a player nobody is holding the controls of.
/// @par Threading
/// Reads the world; call between ticks.

#include <cstdint>
#include <engine/sim/player-input.h>
#include <game/world/game-world.h>

namespace eng::game {

/// How near a hostile actor a stand-in backs away from it, in tiles.
inline constexpr float STAND_IN_WARY_TILES = 4.0F;

/// How far a stand-in lets the player it keeps with get before it follows,
/// in tiles.
inline constexpr float STAND_IN_FOLLOW_TILES = 3.0F;

/// How far a stand-in looks for a hostile actor to aim at, in tiles.
inline constexpr float STAND_IN_AIM_TILES = 12.0F;

/// The input a stand-in gives for the player in input slot @p slot, from
/// @p world as it stands: what the editor's multi-player preview plays the
/// other players with (Editor §7), and what a dropped co-op peer's
/// character will be played with (Game §8).
///
/// Input, not a shortcut into the simulation: it is quantised as a
/// keyboard's is and handed to the tick like anyone's, so a replay records
/// it and a run with stand-ins replays without them. In order, a stand-in
/// goes to a teammate who is down, to revive them; backs away from a
/// hostile actor nearer than `STAND_IN_WARY_TILES`; and keeps within
/// `STAND_IN_FOLLOW_TILES` of the lowest-numbered other player who is up.
/// It aims at the nearest hostile actor in range and never fires — there
/// is nothing to fire yet. A slot with no player, or one who is not up,
/// gives no input.
[[nodiscard]] sim::PlayerInput standInInput(const GameWorld& world,
                                            uint8_t slot);

}  // namespace eng::game
