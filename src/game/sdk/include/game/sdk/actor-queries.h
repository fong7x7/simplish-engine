#pragma once

/// @file actor-queries.h
/// @brief Finding actors: by name, by kind, by where they are.
/// @par Threading
/// Pure reads of the world; call inside the logic's tick.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-actor.h>
#include <game/sdk/actor-filter.h>
#include <optional>
#include <string_view>
#include <vector>

namespace eng::game::sdk {

/// The first actor, in the world's order, named @p id — alive or dying.
[[nodiscard]] std::optional<LogicActor> findActor(const GameLogicWorld& world,
                                                  std::string_view id);

/// Every actor passing @p filter, in the world's order.
[[nodiscard]] std::vector<LogicActor> findActors(const GameLogicWorld& world,
                                                 const ActorFilter& filter);

/// How many actors pass @p filter.
[[nodiscard]] uint32_t countActors(const GameLogicWorld& world,
                                   const ActorFilter& filter);

/// Every actor passing @p filter whose feet are within @p radius tiles of
/// @p at, across the floor (height ignored), in the world's order.
[[nodiscard]] std::vector<LogicActor> actorsWithin(const GameLogicWorld& world,
                                                   Vec3 at, float radius,
                                                   const ActorFilter& filter);

/// The actor passing @p filter nearest @p at across the floor; of two as
/// near, the first in the world's order. Nothing when none passes.
[[nodiscard]] std::optional<LogicActor>
nearestActor(const GameLogicWorld& world, Vec3 at, const ActorFilter& filter);

}  // namespace eng::game::sdk
