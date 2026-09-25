#include <game/fx/combat-fx-preset.h>
#include <game/fx/combat-fx.h>
#include <game/fx/combat-sounds.h>
#include <game/fx/named-fx-effect.h>
#include <span>

namespace eng::game {

std::optional<FxEffect> findNamedFxEffect(std::string_view id) {
  for (uint8_t k = 0; k < COMBAT_CUE_KIND_COUNT; ++k) {
    const auto kind = static_cast<CombatCueKind>(k);
    if (combatSoundName(kind) == id) {
      return combatCueEffect(kind);
    }
  }
  if (const CombatFxPreset* preset = findCombatFxPreset(id)) {
    return FxEffect{.bursts = std::span(&preset->burst, 1),
                    .flash = preset->flash};
  }
  return std::nullopt;
}

}  // namespace eng::game
