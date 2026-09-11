#include <algorithm>
#include <engine/spatial/nav-grid-fit.h>
#include <game/world/world-nav-grid.h>
#include <vector>

namespace eng::game {

spatial::NavGrid buildWorldNavGrid(const GameSetup& setup) {
  const auto players = static_cast<uint8_t>(
      std::clamp<size_t>(setup.player_count, 1, sim::MAX_PLAYERS));
  std::vector<Vec2> points;
  points.reserve(players + setup.actors.size());
  for (uint8_t slot = 0; slot < players; ++slot) {
    points.push_back({setup.spawns[slot].x, setup.spawns[slot].y});
  }
  for (const ActorSpawn& actor : setup.actors) {
    points.push_back({actor.at.x, actor.at.y});
  }
  return {spatial::fitNavGrid(setup.obstacles, points, setup.spawns[0].z),
          setup.obstacles};
}

}  // namespace eng::game
