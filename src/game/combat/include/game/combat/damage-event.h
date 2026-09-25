#pragma once

/// @file damage-event.h
/// @brief One hit, waiting for the damage phase.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/combat/combatant-ref.h>
#include <game/combat/damage-cause.h>

namespace eng::game {

/// Health segments one player or actor is to lose, as whatever hit them
/// recorded it. Nothing writes health directly: a bite, a shot or a blast
/// appends one of these, and the damage phase applies them in the order
/// they were appended (project-format §9) — so what a hit does never
/// depends on when in the tick it happened.
struct DamageEvent {
  /// Who is hit.
  CombatantRef target{};
  /// Segments to take away.
  uint16_t amount = 0;
  /// Who is to be credited with it: the attacker, the shooter, whoever
  /// set off the blast or spilled the pool; `NO_COMBATANT` for nobody.
  CombatantRef source = NO_COMBATANT;
  /// What kind of thing it is.
  DamageCause cause = DamageCause::ATTACK;
};

}  // namespace eng::game
