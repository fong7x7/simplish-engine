#pragma once

/// @file game-world.h
/// @brief The game's simulation state, and its side of every tick.
/// @par Threading
/// Main-thread-only.

#include <cstddef>
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
#include <game/combat/combat-cue.h>
#include <game/combat/combat-effects.h>
#include <game/combat/combat-scene.h>
#include <game/combat/combat-workspace.h>
#include <game/combat/hazard-pool.h>
#include <game/combat/projectile-pool.h>
#include <game/content/behavior-definition.h>
#include <game/content/game-content.h>
#include <game/logic/game-logic.h>
#include <game/logic/run-outcome.h>
#include <game/player/player-pool.h>
#include <game/world/game-setup.h>
#include <game/world/logic-command.h>
#include <game/world/logic-event-log.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng::game {

/// The PCG32 stream actors draw from — wander spots, `chance` conditions —
/// derived from the session seed (Engine REQUIREMENTS §4.3: one named
/// stream per system).
inline constexpr uint64_t AI_RNG_STREAM = 1;

/// The PCG32 stream a project's game logic draws from through
/// `GameLogicWorld::random`: its own, so adding a draw to the logic never
/// shifts what the actors roll.
inline constexpr uint64_t LOGIC_RNG_STREAM = 2;

/// Most lines a world keeps of what its game logic said before whoever
/// runs it takes them; past it, a line is dropped rather than grown into.
inline constexpr size_t WORLD_LOGIC_LOG_LINES = 256;

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
  /// to take in every spawn — and only when there are actors to use it, or
  /// room for game logic to spawn some. @p content is kept too, when the
  /// setup leaves room for actors to be spawned: they name its behaviors
  /// and enemy archetypes.
  ///
  /// @p logic, when given, is a project's own game rules (ADR-011), run in
  /// the director's phase of every tick. The world borrows it: whoever made
  /// it keeps it alive for as long as the world is stepped.
  GameWorld(const GameSetup& setup, const GameContent& content,
            GameLogic* logic = nullptr);

  /// Moves every player by its stick, and keeps them out of the level's
  /// solid geometry. The first phase, so it also empties the last tick's
  /// cues.
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
  /// Runs the project's game logic, when there is one: `start` on tick 0,
  /// then `tick`, then everything it asked for — damage through the same
  /// path as a bite, blasts included.
  void director(const sim::TickContext& context) override;
  /// Destroys what the tick marked for destruction.
  void compaction(const sim::TickContext& context) override;
  /// The players, the actors, the flow fields, the projectiles, the
  /// hazards and the AI stream, one section each — and, with game logic,
  /// a last "logic" section: the outcome, the logic stream and whatever
  /// the logic hashes of its own. A world with no logic hashes exactly as
  /// it did before logic existed, so its replays still verify.
  void hashState(sim::TickHashBuilder& builder) const override;

  /// Whether the run is over: the game logic ended it, or no player is up
  /// (Game §3.3 — solo death ends the run, and a co-op one ends when nobody
  /// is left to revive).
  [[nodiscard]] bool runOver() const;
  /// How the run stands: as the game logic ended it, else lost once no
  /// player is up, else still playing.
  [[nodiscard]] RunOutcome outcome() const;
  /// Whether this world runs a project's game logic.
  [[nodiscard]] bool hasLogic() const { return logic_ != nullptr; }

  /// Every line the game logic has said since the last call, in order,
  /// and forget them — at most `WORLD_LOGIC_LOG_LINES` between calls.
  /// Presentation: never state, never hashed.
  [[nodiscard]] std::vector<std::string> takeLogicLog();

  /// The players, for whatever draws them. Read-only: nothing outside the
  /// tick may change simulation state.
  [[nodiscard]] const PlayerPool& players() const { return players_; }
  /// The actors, for whatever draws them. Read-only.
  [[nodiscard]] const ActorPool& actors() const { return actors_; }
  /// The brains the actors run, indexed by `ActorPool::brain`: what names
  /// the state an actor is in.
  [[nodiscard]] std::span<const ActorBrain> brains() const { return brains_; }
  /// The name the level — or the game logic that spawned it — gave the
  /// actor at dense index @p index; empty for none.
  [[nodiscard]] std::string_view actorId(uint32_t index) const;
  /// What draws the actor at dense index @p index: an asset reference, or
  /// empty for the stand-in. Presentation only.
  [[nodiscard]] std::string_view actorModel(uint32_t index) const;
  /// Whether the actor at dense index @p index was spawned during the run,
  /// rather than being one of the setup's: one with no prop to be drawn as.
  [[nodiscard]] bool actorSpawned(uint32_t index) const;
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

  /// What the last tick's combat did that is worth seeing or hearing —
  /// shots fired, hits, blasts — in the order it happened. Emptied when
  /// the next tick starts, so read it after every tick. Not state, and
  /// never hashed.
  [[nodiscard]] std::span<const CombatCue> combatCues() const { return cues_; }

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
  /// Apply every hit in the effects buffer and every blast they set off,
  /// in order, until neither leaves anything to do.
  void resolveDamage(uint64_t tick);
  /// Run the game logic's part of @p context's tick.
  void runLogic(const sim::TickContext& context);
  /// Call the game logic with @p view on @p tick: `start` first on tick 0.
  void callLogic(GameLogicWorld& view, uint64_t tick);
  /// Apply one of the game logic's queued writes.
  void applyLogicCommand(const LogicCommand& command, uint64_t tick);
  /// Apply one of the game logic's writes to an actor — a move, a removal,
  /// a state or a side — at dense index @p index.
  void applyActorCommand(const LogicCommand& command, uint32_t index,
                         uint64_t tick);
  /// Put the player or actor @p target at @p at.
  void moveTo(const LogicTarget& target, Vec3 at);
  /// Give the player or actor @p target back @p amount health segments.
  void heal(const LogicTarget& target, uint16_t amount);
  /// Apply the game logic's queued writes: damage and healing in order,
  /// the damage resolved, then its spawns in order.
  void applyLogicWrites(uint64_t tick);
  /// Add an actor as @p spawn describes, running the behavior it names;
  /// nothing when the pool is full.
  void addActor(const ActorSpawn& spawn);

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
  /// What the last tick's combat did, for presentation; not state.
  std::vector<CombatCue> cues_;
  /// Who can be hurt this tick, and scratch to find them with; not state.
  CombatWorkspace combat_;
  /// Scratch the actor passes work in; not state.
  ActorWorkspace workspace_;
  /// The AI stream.
  Pcg32 ai_rng_;
  /// A project's game logic, borrowed; null for none.
  GameLogic* logic_ = nullptr;
  /// The game logic's stream.
  Pcg32 logic_rng_;
  /// How the game logic has ended the run; `PLAYING` until it does.
  RunOutcome logic_outcome_ = RunOutcome::PLAYING;
  /// What the logic asked for this tick; empty between ticks.
  std::vector<LogicCommand> logic_commands_;
  /// The actors the logic asked for this tick; empty between ticks.
  std::vector<ActorSpawn> logic_spawns_;
  /// The level's — or the logic's — name for each actor, by its handle's
  /// slot.
  std::vector<std::string> actor_ids_;
  /// What draws each actor, by its handle's slot. Not state.
  std::vector<std::string> actor_models_;
  /// Whether each slot's actor was spawned mid-run, 1 for true. Not state.
  std::vector<uint8_t> actor_spawned_;
  /// What happened last tick, for the game logic; kept only with one.
  LogicEventLog logic_events_;
  /// The run's content, kept only when actors can be spawned mid-run: the
  /// behaviors and archetypes they name. Empty otherwise.
  GameContent content_;
  /// What the game logic has said since `takeLogicLog` last ran; not
  /// state.
  std::vector<std::string> logic_log_;
};

}  // namespace eng::game
