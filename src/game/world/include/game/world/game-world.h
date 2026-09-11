#pragma once

/// @file game-world.h
/// @brief The game's simulation state, and its side of every tick.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/physics/box-broadphase.h>
#include <engine/sim/entity-handle.h>
#include <engine/sim/simulation-systems.h>
#include <engine/sim/tick-context.h>
#include <engine/sim/tick-hash-builder.h>
#include <engine/spatial/nav-grid.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-flow-fields.h>
#include <game/actors/actor-pool.h>
#include <game/actors/actor-route.h>
#include <game/actors/actor-workspace.h>
#include <game/combat/combat-effects.h>
#include <game/combat/combat-scene.h>
#include <game/combat/combat-workspace.h>
#include <game/combat/hazard-pool.h>
#include <game/combat/projectile-pool.h>
#include <game/content/behavior-definition.h>
#include <game/content/game-content.h>
#include <game/player/player-pool.h>
#include <game/world/game-setup.h>
#include <span>
#include <vector>

namespace eng::game {

/// The PCG32 stream actors draw from — wander spots, `chance` conditions —
/// derived from the session seed (Engine REQUIREMENTS §4.3: one named
/// stream per system).
inline constexpr uint64_t AI_RNG_STREAM = 1;

/// Everything the game simulates, and the phases that simulate it — the
/// `SimulationSystems` a `sim::Simulation` steps.
///
/// Players and actors today. Projectiles and the director join as more pools
/// and more phases, in the §4.1 order `Simulation` already calls.
class GameWorld final : public sim::SimulationSystems {
public:
  /// A world at tick 0: one player per slot of @p setup, standing at its
  /// spawn as the character it picked from @p content — or as the default
  /// character, when it picked none the content has — and every actor of
  /// @p setup at its spawn, running the behavior it names. Nothing of
  /// @p content is kept past construction: what a tick needs was copied
  /// into the players and the brains.
  ///
  /// The navigation grid is built here, from the setup's obstacles, sized
  /// to take in every spawn — and only when there are actors to use it.
  GameWorld(const GameSetup& setup, const GameContent& content);

  /// Moves every player by its stick, and keeps them out of the level's
  /// solid geometry.
  void playerControl(const sim::TickContext& context) override;
  /// Every actor perceives, decides, plans, attacks, moves and turns.
  void enemyAi(const sim::TickContext& context) override;
  /// Spawns the projectiles and hazard pools this tick's attacks asked
  /// for.
  void weaponFire(const sim::TickContext& context) override;
  /// Moves every projectile and ages every hazard pool, noting whom they
  /// hit.
  void projectiles(const sim::TickContext& context) override;
  /// Applies every hit of the tick, in the order they happened: blasts
  /// reach everyone near, actors out of health die — some in blasts of
  /// their own — and players out of health go down. Then downed players
  /// are revived or put out.
  void damage(const sim::TickContext& context) override;
  /// Destroys what the tick marked for destruction.
  void compaction(const sim::TickContext& context) override;
  /// The players, the actors, the flow fields, the projectiles, the
  /// hazards and the AI stream, one section each.
  void hashState(sim::TickHashBuilder& builder) const override;

  /// Whether the run is over: no player is up (Game §3.3 — solo death
  /// ends the run, and a co-op one ends when nobody is left to revive).
  [[nodiscard]] bool runOver() const;

  /// The players, for whatever draws them. Read-only: nothing outside the
  /// tick may change simulation state.
  [[nodiscard]] const PlayerPool& players() const { return players_; }
  /// The actors, for whatever draws them. Read-only.
  [[nodiscard]] const ActorPool& actors() const { return actors_; }
  /// The brains the actors run, indexed by `ActorPool::brain`: what names
  /// the state an actor is in.
  [[nodiscard]] std::span<const ActorBrain> brains() const { return brains_; }
  /// Each of the setup's actors' handle, in the setup's order — null for
  /// one the pool had no room for. How whoever built the setup finds the
  /// actor a spawn of theirs became.
  [[nodiscard]] std::span<const sim::EntityHandle> actorHandles() const {
    return actor_handles_;
  }
  /// The navigation grid actors plan across.
  [[nodiscard]] const spatial::NavGrid& navGrid() const { return grid_; }
  /// The projectiles in flight, for whatever draws them. Read-only.
  [[nodiscard]] const ProjectilePool& projectilePool() const {
    return projectiles_;
  }
  /// The hazard pools on the floor, for whatever draws them. Read-only.
  [[nodiscard]] const HazardPool& hazardPool() const { return hazards_; }

private:
  /// Spawn one actor per spawn of @p setup, compiling the brains they run.
  void spawnActors(const GameSetup& setup, const GameContent& content);
  /// This tick's view of the world for the combat phases, with everyone
  /// who can be hurt listed in `combat_`.
  [[nodiscard]] CombatScene combatScene(uint64_t tick);
  /// List everyone who can be hurt — players who are up, actors alive — in
  /// `combat_`.
  void listBodies();
  /// Apply one hit.
  void applyHit(const DamageEvent& hit, uint64_t tick);
  /// Give the actor at dense index @p index the route @p points, when
  /// there is one.
  void assignRoute(uint32_t index, const std::vector<Vec2>& points);
  /// The index in `brains_` of @p behavior, compiled and added if it is
  /// not there yet.
  uint16_t brainIndex(const BehaviorDefinition& behavior);

  /// Every player in the session.
  PlayerPool players_;
  /// The level's solid geometry, from the setup; never changed by a tick.
  std::vector<physics::CollisionBox> obstacles_;
  /// Where actors can go; derived from `obstacles_`, fixed for the run.
  spatial::NavGrid grid_;
  /// `obstacles_`, bucketed for actors to collide with; fixed for the run.
  physics::BoxBroadphase broadphase_;
  /// The brains actors run, each behavior compiled once, in the order
  /// actors first named them.
  std::vector<ActorBrain> brains_;
  /// Every actor in the session.
  ActorPool actors_;
  /// The routes actors patrol, one per actor spawned with one, in setup
  /// order.
  std::vector<ActorRoute> routes_;
  /// Each setup actor's handle, in setup order.
  std::vector<sim::EntityHandle> actor_handles_;
  /// The flow fields actors pursue players by.
  ActorFlowFields flow_;
  /// Projectiles in flight.
  ProjectilePool projectiles_;
  /// Hazard pools on the floor.
  HazardPool hazards_;
  /// What this tick's attacks asked for; empty between ticks.
  CombatEffects effects_;
  /// Who can be hurt this tick, and scratch to find them with; not state.
  CombatWorkspace combat_;
  /// Scratch the actor passes work in; not state.
  ActorWorkspace workspace_;
  /// The AI stream.
  Pcg32 ai_rng_;
};

}  // namespace eng::game
