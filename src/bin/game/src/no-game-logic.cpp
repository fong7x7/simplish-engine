// The game logic of a game built with no project: none. Linked in place of
// a project's own when SIMPLISH_PROJECT_DIR names none, so the executable
// still builds — and CI keeps it building — without one.

#include <game/logic/game-logic.h>

extern "C" eng::game::GameLogic* simplishCreateGameLogic() {
  return nullptr;
}

extern "C" void
simplishDestroyGameLogic([[maybe_unused]] eng::game::GameLogic* logic) {}
