#pragma once

/// @file faction.h
/// @brief Which side an actor is on.
/// @par Threading
/// A value type.

#include <array>
#include <cstdint>

namespace eng::game {

/// Which side an actor is on, which decides who it takes as a target.
///
/// Hostile and friendly actors both take players as their target — what
/// they do about it is their behavior's business: a guard pursues, a
/// follower keeps up — unless their behavior targets opponents, when they
/// take each other on. A neutral actor takes nobody, and goes about its
/// business whoever is near. The players are on the friendly side: what a
/// hostile shot or spit hurts, a friendly one does not.
enum class Faction : uint8_t {
  /// Against the players: the enemies.
  HOSTILE,
  /// Nobody's: ambient life that ignores everyone.
  NEUTRAL,
  /// With the players: companions and townsfolk.
  FRIENDLY,
};

/// Every faction, in the order a choice row steps through them.
inline constexpr std::array<Faction, 3> ALL_FACTIONS{
    Faction::HOSTILE, Faction::NEUTRAL, Faction::FRIENDLY};

/// Whether @p a and @p b are on opposing sides: hostile against friendly.
/// Nobody opposes a neutral.
[[nodiscard]] constexpr bool factionsOppose(Faction a, Faction b) {
  return (a == Faction::HOSTILE && b == Faction::FRIENDLY) ||
         (a == Faction::FRIENDLY && b == Faction::HOSTILE);
}

}  // namespace eng::game
