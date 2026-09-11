#include <engine/core/fixed-step-clock.h>
#include <game/content/character-lookup.h>

namespace eng::game {

const CharacterDefinition& defaultCharacter() {
  static const CharacterDefinition fallback{};
  return fallback;
}

const CharacterDefinition& resolveCharacter(const GameContent& content,
                                            std::string_view id) {
  if (id.empty()) {
    return defaultCharacter();
  }
  for (const CharacterDefinition& character : content.characters) {
    if (character.id == id) {
      return character;
    }
  }
  return defaultCharacter();
}

float characterSpeedPerTick(const CharacterDefinition& character) {
  return character.move_speed / static_cast<float>(TICK_RATE_HZ);
}

}  // namespace eng::game
