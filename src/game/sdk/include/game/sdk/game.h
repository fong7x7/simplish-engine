#pragma once

/// @file game.h
/// @brief The base a project's game logic is easiest written on.
/// @par Threading
/// Main-thread-only; called only from inside a tick.

#include <game/logic/game-logic-hash.h>
#include <game/logic/game-logic-world.h>
#include <game/logic/game-logic.h>
#include <game/logic/logic-event.h>

namespace eng::game::sdk {

/// A `GameLogic` that hands every event of the last tick to a hook of its
/// own before `onTick` — so a rule about deaths is written where deaths
/// are, not as a loop over `world.events()` in every tick. Override what
/// the game needs; every hook does nothing until it is.
///
/// @code
///   class Arena final : public eng::game::sdk::Game {
///     void onActorDied(GameLogicWorld& world, const LogicEvent& death)
///     override {
///       score_ += 1;
///     }
///     void onHash(GameLogicHash& hash) const override { hash.add(score_); }
///     uint32_t score_ = 0;
///   };
///   SIMPLISH_GAME_LOGIC(Arena)
/// @endcode
///
/// Hooks run in the order the events happened, then `onTick` — or, while
/// the game is paused, `onPausedTick`.
class Game : public GameLogic {
public:
  void start(GameLogicWorld& world) final;
  void tick(GameLogicWorld& world) final;
  void hashState(GameLogicHash& hash) const final;
  void end(GameLogicWorld& world) final;

protected:
  /// Once, on tick 0, before anything else.
  virtual void onStart([[maybe_unused]] GameLogicWorld& world) {}
  /// Every tick played, after the last tick's events have had their hooks.
  /// Not while paused: time gameplay by `world.playTick()`, which stands
  /// still then, and each value of it reaches here once.
  virtual void onTick([[maybe_unused]] GameLogicWorld& world) {}
  /// Every tick the game is paused, in place of `onTick`: for what a
  /// pause screen shows.
  virtual void onPausedTick([[maybe_unused]] GameLogicWorld& world) {}
  /// An actor the logic spawned joined the run.
  virtual void onActorSpawned([[maybe_unused]] GameLogicWorld& world,
                              [[maybe_unused]] const LogicEvent& event) {}
  /// An actor was hurt, and lived.
  virtual void onActorHurt([[maybe_unused]] GameLogicWorld& world,
                           [[maybe_unused]] const LogicEvent& event) {}
  /// An actor was killed. It is gone; the event says where it fell.
  virtual void onActorDied([[maybe_unused]] GameLogicWorld& world,
                           [[maybe_unused]] const LogicEvent& event) {}
  /// An actor was removed by the logic.
  virtual void onActorRemoved([[maybe_unused]] GameLogicWorld& world,
                              [[maybe_unused]] const LogicEvent& event) {}
  /// A player was hurt, and is still up.
  virtual void onPlayerHurt([[maybe_unused]] GameLogicWorld& world,
                            [[maybe_unused]] const LogicEvent& event) {}
  /// A player went down.
  virtual void onPlayerDowned([[maybe_unused]] GameLogicWorld& world,
                              [[maybe_unused]] const LogicEvent& event) {}
  /// A downed player was brought back up; `event.by` is who did it.
  virtual void onPlayerRevived([[maybe_unused]] GameLogicWorld& world,
                               [[maybe_unused]] const LogicEvent& event) {}
  /// A downed player is out of the run.
  virtual void onPlayerOut([[maybe_unused]] GameLogicWorld& world,
                           [[maybe_unused]] const LogicEvent& event) {}
  /// An actor went into another state; `event.state` is its id.
  virtual void onActorStateEntered([[maybe_unused]] GameLogicWorld& world,
                                   [[maybe_unused]] const LogicEvent& event) {}
  /// An actor took someone new as its target; `event.other` is whom.
  virtual void onActorNoticed([[maybe_unused]] GameLogicWorld& world,
                              [[maybe_unused]] const LogicEvent& event) {}
  /// An actor attacked; `event.other` is whom it had in mind.
  virtual void onActorAttacked([[maybe_unused]] GameLogicWorld& world,
                               [[maybe_unused]] const LogicEvent& event) {}
  /// An actor began an attack that winds up; it lands, if it still can,
  /// its behavior's `windup_ticks` later.
  virtual void onActorWindingUp([[maybe_unused]] GameLogicWorld& world,
                                [[maybe_unused]] const LogicEvent& event) {}
  /// A player's foot came down; heard while listening for steps.
  virtual void onPlayerStepped([[maybe_unused]] GameLogicWorld& world,
                               [[maybe_unused]] const LogicEvent& event) {}
  /// An actor's foot came down; heard while listening for everyone's.
  virtual void onActorStepped([[maybe_unused]] GameLogicWorld& world,
                              [[maybe_unused]] const LogicEvent& event) {}
  /// A player chose an action on one of the game's screens; `event.id` is
  /// the action, `event.target` the player.
  virtual void onUiAction([[maybe_unused]] GameLogicWorld& world,
                          [[maybe_unused]] const LogicEvent& event) {}
  /// A player pressed the pause button; `event.target` is who. Nothing
  /// pauses unless the logic says so: `sdk::togglePause(world, "pause")`.
  virtual void onPausePressed([[maybe_unused]] GameLogicWorld& world,
                              [[maybe_unused]] const LogicEvent& event) {}
  /// Once, when the run is over — `world.outcome()` says how. Log the
  /// run's tally here; writes do nothing.
  virtual void onRunEnded([[maybe_unused]] GameLogicWorld& world) {}
  /// Fold every member a later tick decides anything by into @p hash.
  virtual void onHash([[maybe_unused]] GameLogicHash& hash) const {}

private:
  /// Hand @p event to its hook.
  void dispatch(GameLogicWorld& world, const LogicEvent& event);
};

}  // namespace eng::game::sdk
