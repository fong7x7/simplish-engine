#pragma once

/// @file world-logic-view.h
/// @brief The world, behind the interface a project's game logic calls.
/// @par Threading
/// Main-thread-only; lives for one run of the logic.

#include "world-logic-scene.h"

#include <cstdint>
#include <engine/sim/tick-input.h>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-actor.h>
#include <game/logic/logic-player.h>
#include <game/logic/logic-target.h>
#include <game/logic/run-outcome.h>
#include <string_view>

namespace eng::game {

/// `GameLogicWorld` over a `GameWorld`'s pools: reads copy out of them,
/// writes queue a `LogicCommand` the world applies when the logic returns.
class WorldLogicView final : public GameLogicWorld {
public:
  /// A view of @p scene, which must outlive it.
  explicit WorldLogicView(const WorldLogicScene& scene) : scene_(scene) {}

  [[nodiscard]] uint64_t tick() const override;
  [[nodiscard]] const sim::TickInput& input() const override;
  [[nodiscard]] uint32_t playerCount() const override;
  [[nodiscard]] LogicPlayer player(uint32_t index) const override;
  [[nodiscard]] uint32_t actorCount() const override;
  [[nodiscard]] LogicActor actor(uint32_t index) const override;
  [[nodiscard]] RunOutcome outcome() const override;
  void damage(LogicTarget target, uint16_t amount) override;
  void heal(LogicTarget target, uint16_t amount) override;
  void endRun(RunOutcome outcome) override;
  [[nodiscard]] uint32_t random(uint32_t bound) override;
  void log(std::string_view message) override;

private:
  /// The world, borrowed.
  WorldLogicScene scene_;
};

}  // namespace eng::game
