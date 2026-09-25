#pragma once

/// @file logic-combatant.h
/// @brief Game logic's names for the players and actors combat names.
/// @par Threading
/// Pure functions.

#include <game/combat/combatant-ref.h>
#include <game/combat/damage-cause.h>
#include <game/logic/logic-damage-cause.h>
#include <game/logic/logic-target.h>
#include <optional>

namespace eng::game {

/// @p ref as game logic names it; nothing for `NO_COMBATANT`.
[[nodiscard]] std::optional<LogicTarget> logicTargetOf(const CombatantRef& ref);

/// @p target as combat names it; `NO_COMBATANT` for nothing.
[[nodiscard]] CombatantRef
combatantOf(const std::optional<LogicTarget>& target);

/// @p cause as game logic hears it.
[[nodiscard]] LogicDamageCause logicCauseOf(DamageCause cause);

}  // namespace eng::game
