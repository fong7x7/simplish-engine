#include <game/combat/combat-cue.h>

namespace eng::game {

void cueCombat(std::vector<CombatCue>& cues, const CombatCue& cue) {
  if (cues.size() < cues.capacity()) {
    cues.push_back(cue);
  }
}

}  // namespace eng::game
