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
  [[nodiscard]] std::optional<LogicActor>
  actorOf(LogicTarget target) const override;
  [[nodiscard]] std::optional<LogicPlayer>
  playerOf(LogicTarget target) const override;
  [[nodiscard]] std::span<const LogicEvent> events() const override;
  [[nodiscard]] bool lineOfSight(Vec3 from, Vec3 to) const override;
  [[nodiscard]] bool walkable(Vec3 at) const override;
  [[nodiscard]] uint32_t obstacleCount() const override;
  [[nodiscard]] physics::CollisionBox obstacle(uint32_t index) const override;
  using GameLogicWorld::damage;
  void damage(LogicTarget target, uint16_t amount,
              std::optional<LogicTarget> by) override;
  void heal(LogicTarget target, uint16_t amount) override;
  void endRun(RunOutcome outcome) override;
  bool spawnEnemy(std::string_view archetype, Vec3 at,
                  std::string_view id) override;
  bool spawnActor(const LogicSpawn& spawn) override;
  [[nodiscard]] uint32_t actorRoom() const override;
  void moveTo(LogicTarget target, Vec3 at) override;
  void removeActor(LogicTarget target) override;
  bool setActorState(LogicTarget target, std::string_view state) override;
  void setActorFaction(LogicTarget target, Faction faction) override;
  void fireShot(const LogicShot& shot) override;
  void blast(const LogicBlast& blast) override;
  void spawnHazard(const LogicHazard& hazard) override;
  [[nodiscard]] uint32_t random(uint32_t bound) override;
  void log(std::string_view message) override;

private:
  /// The dense index of the actor @p target names, if it is one still in
  /// the pool.
  [[nodiscard]] std::optional<uint32_t> actorIndex(LogicTarget target) const;
  /// The clearance an actor of the default size needs of the grid.
  [[nodiscard]] uint8_t clearance() const;
  /// Queue @p spawn when there is room. False when there is none.
  bool queueSpawn(ActorSpawn spawn);

  /// The world, borrowed.
  WorldLogicScene scene_;
};

}  // namespace eng::game
