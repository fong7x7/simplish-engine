#include <algorithm>
#include <game/content/character-lookup.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>

namespace eng::game {

GameWorld::GameWorld(const GameSetup& setup, const GameContent& content)
  : obstacles_(setup.obstacles) {
  const auto count = static_cast<uint8_t>(
      std::clamp<size_t>(setup.player_count, 1, sim::MAX_PLAYERS));
  for (uint8_t slot = 0; slot < count; ++slot) {
    (void)spawnPlayer(players_, slot, setup.spawns[slot],
                      resolveCharacter(content, setup.characters[slot]));
  }
}

void GameWorld::playerControl(const sim::TickContext& context) {
  movePlayers(players_, context.input, obstacles_);
}

void GameWorld::compaction([[maybe_unused]] const sim::TickContext& context) {
  compactPlayers(players_);
}

void GameWorld::hashState(sim::TickHashBuilder& builder) const {
  hashPlayers(players_, builder.section("players"));
}

}  // namespace eng::game
