#pragma once

/// @file game-logic-world.h
/// @brief What game logic can read of the world, and change in it.
/// @par Threading
/// Main-thread-only; valid only during `GameLogic::start` and `tick`.

#include <cstdint>
#include <engine/sim/tick-input.h>
#include <game/logic/logic-actor.h>
#include <game/logic/logic-player.h>
#include <game/logic/logic-target.h>
#include <game/logic/run-outcome.h>
#include <string_view>

namespace eng::game {

/// The world, as a project's game logic sees it during its phase of a tick
/// (Engine §4.1 step 7, where the director runs).
///
/// **Reads** see the world as the tick's damage phase left it: players have
/// moved, actors have acted, hits have landed. They do not see this tick's
/// writes, so the order logic reads things in never matters.
///
/// **Writes** are queued, and applied in the order they were made once
/// `GameLogic::tick` returns: damage through the same path an actor's bite
/// takes — a player's grace after a hit, an actor's death and any blast it
/// goes off in — and healing capped at a full bar. An actor killed this way
/// is gone at the end of the tick, as one killed by a shot is.
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

  /// Take @p amount health segments from @p target. Nothing, when it is
  /// gone.
  virtual void damage(LogicTarget target, uint16_t amount) = 0;
  /// Give @p target back @p amount health segments, up to a full bar. A
  /// player who is down or out is not healed: reviving is a teammate's.
  virtual void heal(LogicTarget target, uint16_t amount) = 0;
  /// End the run as @p outcome — `WON` or `LOST`; `PLAYING` does nothing.
  /// The first ending stands.
  virtual void endRun(RunOutcome outcome) = 0;

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
