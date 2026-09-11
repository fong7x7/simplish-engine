#pragma once

/// @file damage-event.h
/// @brief One hit, waiting for the damage phase.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/combat/combatant-ref.h>

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
};

}  // namespace eng::game
