#pragma once

/// @file weapon.h
/// @brief A gun: what a player fires when they hold fire.
/// @par Threading
/// A value type; `fireWeapon` writes to the world inside the logic's tick.

#include <cstdint>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-player.h>
#include <game/sdk/cooldown.h>

namespace eng::game::sdk {

/// What one pull of the trigger fires: `pellets` shots fanned evenly
/// across `spread_degrees`, centred on the player's aim, then nothing
/// until `refire_ticks` have passed.
///
/// @code
///   constexpr sdk::Weapon RIFLE{.damage = 1, .refire_ticks = 8};
///   constexpr sdk::Weapon SHOTGUN{.pellets = 5, .spread_degrees = 30,
///                                 .refire_ticks = 40};
///   sdk::EntityData<sdk::Cooldown> triggers_;
///   for (const auto& player : sdk::playersUp(world))
///     sdk::fireWeapon(world, player, RIFLE, triggers_[player.target]);
/// @endcode
struct Weapon {
  /// Health segments each shot takes.
  uint16_t damage = 1;
  /// How fast its shots fly, in tiles a second.
  float speed = 24.0F;
  /// Ticks between pulls: 8 is 7.5 a second.
  uint64_t refire_ticks = 8;
  /// Shots a pull fires.
  uint32_t pellets = 1;
  /// How wide they fan, in degrees, first to last.
  float spread_degrees = 0.0F;
};

/// Fire @p weapon for @p player when they hold fire and @p cooldown is
/// ready — from the edge of them, along their aim, on the players' side,
/// credited to them — and start @p cooldown. Whether it fired.
bool fireWeapon(GameLogicWorld& world, const LogicPlayer& player,
                const Weapon& weapon, Cooldown& cooldown);

}  // namespace eng::game::sdk
