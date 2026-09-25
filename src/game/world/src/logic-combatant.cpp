#include <game/world/logic-combatant.h>

namespace eng::game {

std::optional<LogicTarget> logicTargetOf(const CombatantRef& ref) {
  if (ref.kind == NO_COMBATANT.kind && ref.handle == NO_COMBATANT.handle) {
    return std::nullopt;
  }
  return LogicTarget{ref.kind == CombatantKind::PLAYER ? LogicTargetKind::PLAYER
                                                       : LogicTargetKind::ACTOR,
                     ref.handle.index, ref.handle.generation};
}

CombatantRef combatantOf(const std::optional<LogicTarget>& target) {
  if (!target) {
    return NO_COMBATANT;
  }
  return {target->kind == LogicTargetKind::PLAYER ? CombatantKind::PLAYER
                                                  : CombatantKind::ACTOR,
          {target->index, target->generation}};
}

}  // namespace eng::game
