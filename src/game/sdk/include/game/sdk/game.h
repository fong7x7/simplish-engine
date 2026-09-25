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
/// Hooks run in the order the events happened, then `onTick`.
class Game : public GameLogic {
public:
  void start(GameLogicWorld& world) final;
  void tick(GameLogicWorld& world) final;
  void hashState(GameLogicHash& hash) const final;

protected:
  /// Once, on tick 0, before anything else.
  virtual void onStart([[maybe_unused]] GameLogicWorld& world) {}
  /// Every tick, after the last tick's events have had their hooks.
  virtual void onTick([[maybe_unused]] GameLogicWorld& world) {}
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
  /// Fold every member a later tick decides anything by into @p hash.
  virtual void onHash([[maybe_unused]] GameLogicHash& hash) const {}

private:
  /// Hand @p event to its hook.
  void dispatch(GameLogicWorld& world, const LogicEvent& event);
};

}  // namespace eng::game::sdk
