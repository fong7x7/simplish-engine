#include <game/content/enemy-lookup.h>

namespace eng::game {

const EnemyDefinition* findEnemy(const GameContent& content,
                                 std::string_view id) {
  for (const EnemyDefinition& enemy : content.enemies) {
    if (enemy.id == id) {
      return &enemy;
    }
  }
  return nullptr;
}

}  // namespace eng::game
