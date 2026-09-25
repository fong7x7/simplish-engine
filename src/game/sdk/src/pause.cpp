#include <game/sdk/pause.h>

namespace eng::game::sdk {

bool togglePause(GameLogicWorld& world, std::string_view screen) {
  if (world.paused()) {
    world.resume();
    if (!screen.empty()) {
      world.hideScreen(screen);
    }
    return false;
  }
  world.pause();
  if (!screen.empty()) {
    world.showScreen(screen);
  }
  return true;
}

}  // namespace eng::game::sdk
