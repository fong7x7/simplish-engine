#pragma once

/// @file game-logic-world.h
/// @brief What game logic can read of the world, and change in it.
/// @par Threading
/// Main-thread-only; valid only during `GameLogic::start` and `tick`.

#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/physics/collision-box.h>
#include <engine/sim/tick-input.h>
#include <game/content/faction.h>
#include <game/logic/logic-actor.h>
#include <game/logic/logic-blast.h>
#include <game/logic/logic-event.h>
#include <game/logic/logic-hazard.h>
#include <game/logic/logic-player.h>
#include <game/logic/logic-shot.h>
#include <game/logic/logic-spawn.h>
#include <game/logic/logic-target.h>
#include <game/logic/run-outcome.h>
#include <optional>
#include <span>
#include <string_view>

namespace eng::game {

/// The world, as a project's game logic sees it during its phase of a tick
/// (Engine §4.1 step 7, where the director runs).
///
/// **Reads** see the world as the tick's damage phase left it: players have
/// moved, actors have acted, hits have landed. They do not see this tick's
/// writes, so the order logic reads things in never matters.
///
/// **Writes** are queued, and applied once `GameLogic::tick` returns:
/// damage, healing, moves, removals, states and factions first, in the
/// order they were made — damage through the same path an actor's bite
/// takes, a player's grace after a hit, an actor's death and any blast it
/// goes off in; healing capped at a full bar — then blasts, then shots and
/// hazard pools, then spawns, each in the order they were made. An actor killed
/// this way is gone at the end of the tick, as one killed by a shot is; one
/// spawned is read from the next tick on.
///
/// Randomness comes from `random` and nowhere else: it draws on the run's
/// own logic stream, derived from the session seed, so two peers draw the
/// same numbers. Anything else — a clock, `std::rand`, the address of an
/// object, iterating an unordered container — makes the run a function of
/// something a replay does not record (ADR-002).
///
/// An interface, so a project's logic calls into the host through a table
/// of functions and links against nothing of the engine's.
class GameLogicWorld {
public:
  virtual ~GameLogicWorld() = default;

  /// The tick being simulated, counted from 0. A tick is 1/60 s, so this is
  /// the clock.
  [[nodiscard]] virtual uint64_t tick() const = 0;
  /// Every player's input for this tick, by input slot.
  [[nodiscard]] virtual const sim::TickInput& input() const = 0;

  /// Players in the session.
  [[nodiscard]] virtual uint32_t playerCount() const = 0;
  /// The player at @p index, 0 to `playerCount() - 1`.
  [[nodiscard]] virtual LogicPlayer player(uint32_t index) const = 0;
  /// Actors in the world, alive or dying this tick.
  [[nodiscard]] virtual uint32_t actorCount() const = 0;
  /// The actor at @p index, 0 to `actorCount() - 1`. An index names a
  /// different actor next tick; keep its `target` instead.
  [[nodiscard]] virtual LogicActor actor(uint32_t index) const = 0;
  /// How the run stands.
  [[nodiscard]] virtual RunOutcome outcome() const = 0;
  /// The actor @p target names, as it is now; nothing when it is gone or
  /// is a player.
  [[nodiscard]] virtual std::optional<LogicActor>
  actorOf(LogicTarget target) const = 0;
  /// The player @p target names, as they are now; nothing when it names
  /// no player.
  [[nodiscard]] virtual std::optional<LogicPlayer>
  playerOf(LogicTarget target) const = 0;
  /// Everything that happened in the last tick, in the order it was
  /// noticed: actors spawned, hurt, killed and removed; players hurt and
  /// downed. Empty on tick 0. The events of the tick before are gone.
  [[nodiscard]] virtual std::span<const LogicEvent> events() const = 0;

  /// Whether an actor of the default size could see from @p from to @p to
  /// across the level's navigation grid: no solid prop between them.
  /// False where the run has no grid — no actors, and no room for any.
  [[nodiscard]] virtual bool lineOfSight(Vec3 from, Vec3 to) const = 0;
  /// Whether an actor of the default size could stand at @p at: inside
  /// the navigation grid, and clear of the props. False with no grid.
  [[nodiscard]] virtual bool walkable(Vec3 at) const = 0;
  /// The level's solid props, as boxes: how many there are.
  [[nodiscard]] virtual uint32_t obstacleCount() const = 0;
  /// The @p index-th solid box, 0 to `obstacleCount() - 1`.
  [[nodiscard]] virtual physics::CollisionBox
  obstacle(uint32_t index) const = 0;

  /// Take @p amount health segments from @p target, crediting @p by —
  /// who is behind it, when anyone is. Nothing, when the target is gone.
  virtual void damage(LogicTarget target, uint16_t amount,
                      std::optional<LogicTarget> by) = 0;
  /// Take @p amount health segments from @p target, crediting nobody.
  void damage(LogicTarget target, uint16_t amount) {
    damage(target, amount, std::nullopt);
  }
  /// Give @p target back @p amount health segments, up to a full bar. A
  /// player who is down or out is not healed: reviving is a teammate's.
  virtual void heal(LogicTarget target, uint16_t amount) = 0;
  /// End the run as @p outcome — `WON` or `LOST`; `PLAYING` does nothing.
  /// The first ending stands.
  virtual void endRun(RunOutcome outcome) = 0;

  /// Add one of the project's enemy archetypes — its health, body,
  /// behavior, side and model, from the enemies table — standing at @p at,
  /// named @p id. False, and nothing queued, when the project has no such
  /// archetype or the run has no room left (`actorRoom`).
  virtual bool spawnEnemy(std::string_view archetype, Vec3 at,
                          std::string_view id) = 0;
  /// Add an actor as @p spawn describes. False, and nothing queued, when
  /// the run has no room left.
  virtual bool spawnActor(const LogicSpawn& spawn) = 0;
  /// Put the player or actor @p target at @p at, as if it had always been
  /// there: an actor forgets the path it was walking. Queued, like damage.
  virtual void moveTo(LogicTarget target, Vec3 at) = 0;
  /// Take the actor @p target out of the run without its dying — no blast
  /// goes off, and it is reported as removed, not killed. Queued.
  virtual void removeActor(LogicTarget target) = 0;
  /// Put the actor @p target into the state of its behavior called
  /// @p state, from the start of it, as if an exit had led there. False,
  /// and nothing queued, when it is gone or its behavior has no such state.
  virtual bool setActorState(LogicTarget target, std::string_view state) = 0;
  /// Put the actor @p target on @p faction's side. Queued.
  virtual void setActorFaction(LogicTarget target, Faction faction) = 0;

  /// Fire @p shot: a projectile in flight from the next tick on, cued as
  /// fired for the sound and the flash. Queued; dropped when the world
  /// already has as many in flight as it holds.
  virtual void fireShot(const LogicShot& shot) = 0;
  /// Set off @p blast: its hits land this tick, with the logic's damage,
  /// and it is cued for the fireball. Queued.
  virtual void blast(const LogicBlast& blast) = 0;
  /// Leave @p hazard on the floor, biting from the next tick on. Queued.
  virtual void spawnHazard(const LogicHazard& hazard) = 0;

  /// How many more actors can be spawned this tick: the run's room, less
  /// the actors in it — the dying among them, until the tick ends — and
  /// the spawns already queued.
  [[nodiscard]] virtual uint32_t actorRoom() const = 0;

  /// A number from 0 to @p bound - 1 from the run's logic stream; 0 when
  /// @p bound is 0.
  [[nodiscard]] virtual uint32_t random(uint32_t bound) = 0;
  /// Say @p message where whoever is running the game reads its log — the
  /// editor's, or the deployed game's output. Presentation: never state.
  virtual void log(std::string_view message) = 0;

  GameLogicWorld(const GameLogicWorld&) = delete;
  GameLogicWorld& operator=(const GameLogicWorld&) = delete;
  GameLogicWorld(GameLogicWorld&&) = delete;
  GameLogicWorld& operator=(GameLogicWorld&&) = delete;

protected:
  GameLogicWorld() = default;
};

}  // namespace eng::game
