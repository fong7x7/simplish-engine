#pragma once

/// @file game-world.h
/// @brief The game's simulation state, and its side of every tick.
/// @par Threading
/// Main-thread-only.

#include <engine/sim/simulation-systems.h>
#include <engine/sim/tick-context.h>
#include <engine/sim/tick-hash-builder.h>
#include <game/player/player-pool.h>
#include <game/world/game-setup.h>

namespace eng::game {

/// Everything the game simulates, and the phases that simulate it — the
/// `SimulationSystems` a `sim::Simulation` steps.
///
/// Only players today. Enemies, projectiles and the director join as more
/// pools and more phases, in the §4.1 order `Simulation` already calls.
class GameWorld final : public sim::SimulationSystems {
public:
  /// A world at tick 0: one player per slot of @p setup, standing at its
  /// spawn.
  explicit GameWorld(const GameSetup& setup);

  /// Moves every player by its stick.
  void playerControl(const sim::TickContext& context) override;
  /// Destroys what the tick marked for destruction.
  void compaction(const sim::TickContext& context) override;
  /// The players, as one section.
  void hashState(sim::TickHashBuilder& builder) const override;

  /// The players, for whatever draws them. Read-only: nothing outside the
  /// tick may change simulation state.
  [[nodiscard]] const PlayerPool& players() const { return players_; }

private:
  /// Every player in the session.
  PlayerPool players_;
};

}  // namespace eng::game
