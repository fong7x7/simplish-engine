#include <cstddef>
#include <game/sdk/game.h>
#include <iterator>

namespace eng::game::sdk {

void Game::start(GameLogicWorld& world) {
  onStart(world);
}

void Game::tick(GameLogicWorld& world) {
  for (const LogicEvent& event : world.events()) {
    dispatch(world, event);
  }
  onTick(world);
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
      &Game::onActorSpawned,  &Game::onActorHurt,    &Game::onActorDied,
      &Game::onActorRemoved,  &Game::onPlayerHurt,   &Game::onPlayerDowned,
      &Game::onPlayerRevived, &Game::onPlayerOut,    &Game::onActorStateEntered,
      &Game::onActorNoticed,  &Game::onActorAttacked};
  static_assert(std::size(HOOKS) ==
                static_cast<size_t>(LogicEventKind::ACTOR_ATTACKED) + 1);
  (this->*HOOKS[static_cast<size_t>(event.kind)])(world, event);
}

}  // namespace eng::game::sdk
