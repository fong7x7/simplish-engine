#include <game/sdk/player-queries.h>

namespace eng::game::sdk {

std::vector<LogicPlayer> playersUp(const GameLogicWorld& world) {
  std::vector<LogicPlayer> up;
  for (uint32_t i = 0; i < world.playerCount(); ++i) {
    if (LogicPlayer player = world.player(i);
        player.status == LogicPlayerStatus::UP) {
      up.push_back(player);
    }
  }
  return up;
}

std::optional<LogicPlayer> nearestPlayer(const GameLogicWorld& world, Vec3 at) {
  std::optional<LogicPlayer> nearest;
  float best = 0.0F;
  for (const LogicPlayer& player : playersUp(world)) {
    const float dx = player.position.x - at.x;
    const float dy = player.position.y - at.y;
    const float d = dx * dx + dy * dy;
    if (!nearest || d < best) {
      nearest = player;
      best = d;
    }
  }
  return nearest;
}

Vec3 playersCentre(const GameLogicWorld& world) {
  const std::vector<LogicPlayer> up = playersUp(world);
  if (up.empty()) {
    return world.playerCount() > 0 ? world.player(0).position : Vec3{};
  }
  Vec3 sum{};
  for (const LogicPlayer& player : up) {
    sum = sum + player.position;
  }
  return sum / static_cast<float>(up.size());
}

}  // namespace eng::game::sdk
