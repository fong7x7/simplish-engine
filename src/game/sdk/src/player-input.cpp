#include <engine/input/input-action.h>
#include <game/sdk/player-input.h>

namespace eng::game::sdk {

bool firing(const GameLogicWorld& world, const LogicPlayer& player) {
  return !world.paused() && player.status == LogicPlayerStatus::UP &&
         player.slot < world.input().players.size() &&
         (world.input().players[player.slot].buttons &
          input::INPUT_BUTTON_FIRE) != 0;
}

}  // namespace eng::game::sdk
