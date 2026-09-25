#pragma once

/// @file player-queries.h
/// @brief Finding players: who is still up, and who is nearest.
/// @par Threading
/// Pure reads of the world; call inside the logic's tick.

#include <engine/math/vec3.h>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-player.h>
#include <optional>
#include <vector>

namespace eng::game::sdk {

/// Every player who is up, in the world's order.
[[nodiscard]] std::vector<LogicPlayer> playersUp(const GameLogicWorld& world);

/// The player who is up nearest @p at across the floor; of two as near,
/// the first. Nothing when nobody is up.
[[nodiscard]] std::optional<LogicPlayer>
nearestPlayer(const GameLogicWorld& world, Vec3 at);

/// Where the players who are up stand, on average; the first player's
/// spot when nobody is up. Where to centre a wave on.
[[nodiscard]] Vec3 playersCentre(const GameLogicWorld& world);

}  // namespace eng::game::sdk
