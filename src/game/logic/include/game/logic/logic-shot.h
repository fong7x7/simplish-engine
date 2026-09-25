#pragma once

/// @file logic-shot.h
/// @brief A projectile game logic fires.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <game/content/faction.h>
#include <game/logic/logic-target.h>
#include <optional>

namespace eng::game {

/// One projectile, for `GameLogicWorld::fireShot`: it flies as an actor's
/// volley does — straight, for three seconds, until it strikes a prop or a
/// body of the other side.
struct LogicShot {
  /// Where it leaves from, in tiles. Height is ignored: projectiles fly at
  /// chest height.
  Vec3 from{};
  /// Which way it flies, world X and Y; need not be unit length. A zero
  /// direction fires nothing.
  Vec2 direction{1.0F, 0.0F};
  /// How fast, in tiles a second.
  float speed = 24.0F;
  /// Health segments it takes from what it strikes.
  uint16_t damage = 1;
  /// Whose shot it is: it strikes only the other side. Players are
  /// friendly, so a player's shot is `FRIENDLY`.
  Faction side = Faction::FRIENDLY;
  /// Who fired it: credited with what it hits. Empty for nobody.
  std::optional<LogicTarget> shooter{};
};

}  // namespace eng::game
