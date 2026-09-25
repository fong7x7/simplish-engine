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

LogicDamageCause logicCauseOf(DamageCause cause) {
  // The logic's list is combat's, after NONE, in the same order.
  static_assert(static_cast<uint8_t>(LogicDamageCause::LOGIC) ==
                DAMAGE_CAUSE_COUNT);
  static_assert(static_cast<uint8_t>(DamageCause::LOGIC) + 1 ==
                DAMAGE_CAUSE_COUNT);
  return static_cast<LogicDamageCause>(static_cast<uint8_t>(cause) + 1);
}

}  // namespace eng::game
