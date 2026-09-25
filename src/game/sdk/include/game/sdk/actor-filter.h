#pragma once

/// @file actor-filter.h
/// @brief Which actors a query is about.
/// @par Threading
/// A value type; its view must outlive the query it is used in.

#include <game/content/faction.h>
#include <game/logic/logic-actor.h>
#include <game/sdk/actor-life.h>
#include <optional>
#include <string_view>

namespace eng::game::sdk {

/// A test every actor a query returns passes. The default takes every
/// actor alive.
///
/// @code
///   sdk::countActors(world, {.faction = eng::game::Faction::HOSTILE});
///   sdk::findActors(world, {.id_prefix = "wave3"});
/// @endcode
struct ActorFilter {
  /// Only actors on this side; any side when empty.
  std::optional<Faction> faction{};
  /// Only actors whose id starts with this; any when empty.
  std::string_view id_prefix{};
  /// Only the living, or the dying too.
  ActorLife life = ActorLife::ALIVE;
};

/// Whether @p actor passes @p filter.
[[nodiscard]] bool matches(const ActorFilter& filter, const LogicActor& actor);

}  // namespace eng::game::sdk
