#pragma once

/// @file player-queries.h
/// @brief Finding players: who is still up, and who is nearest.
/// @par Threading
/// Pure reads of the world; call inside the logic's tick.

#include <engine/math/vec3.h>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-event.h>
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

/// The player credited with @p event — who shot, struck or set off what
/// hurt, killed or downed — when that was a player still in the run.
/// Nothing for an event nobody or an actor is credited with.
///
/// @code
///   void onActorDied(GameLogicWorld& world, const LogicEvent& death) override
///   {
///     if (const auto killer = sdk::playerBehind(world, death))
///       kills_[killer->target] += 1;
///   }
/// @endcode
[[nodiscard]] std::optional<LogicPlayer>
playerBehind(const GameLogicWorld& world, const LogicEvent& event);

/// Where the players who are up stand, on average; the first player's
/// spot when nobody is up. Where to centre a wave on.
[[nodiscard]] Vec3 playersCentre(const GameLogicWorld& world);

}  // namespace eng::game::sdk
