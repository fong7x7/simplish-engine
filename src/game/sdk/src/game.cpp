#include <cstddef>
#include <game/sdk/game.h>
#include <iterator>

namespace eng::game::sdk {

void Game::start(GameLogicWorld& world) {
  onStart(world);
}

void Game::tick(GameLogicWorld& world) {
  // Decided before the hooks, so the tick a menu resumes on is still a
  // paused one: each play tick reaches onTick once, never twice.
  const bool paused = world.paused();
  for (const LogicEvent& event : world.events()) {
    dispatch(world, event);
  }
  if (paused) {
    onPausedTick(world);
  } else {
    onTick(world);
  }
}

void Game::hashState(GameLogicHash& hash) const {
  onHash(hash);
}

void Game::end(GameLogicWorld& world) {
  onRunEnded(world);
}

void Game::dispatch(GameLogicWorld& world, const LogicEvent& event) {
  // In `LogicEventKind` order; the assertion catches a kind added without
  // a hook.
  using Hook = void (Game::*)(GameLogicWorld&, const LogicEvent&);
  static constexpr Hook HOOKS[] = {
      &Game::onActorSpawned,      &Game::onActorHurt,
      &Game::onActorDied,         &Game::onActorRemoved,
      &Game::onPlayerHurt,        &Game::onPlayerDowned,
      &Game::onPlayerRevived,     &Game::onPlayerOut,
      &Game::onActorStateEntered, &Game::onActorNoticed,
      &Game::onActorAttacked,     &Game::onActorWindingUp,
      &Game::onPlayerStepped,     &Game::onActorStepped,
      &Game::onUiAction,          &Game::onPausePressed};
  static_assert(std::size(HOOKS) == LOGIC_EVENT_KIND_COUNT);
  (this->*HOOKS[static_cast<size_t>(event.kind)])(world, event);
}

}  // namespace eng::game::sdk
