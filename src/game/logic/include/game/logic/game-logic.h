#pragma once

/// @file game-logic.h
/// @brief A project's own game rules, written in C++.
/// @par Threading
/// Main-thread-only; called only from inside a tick.

#include <game/logic/game-logic-hash.h>
#include <game/logic/game-logic-world.h>

namespace eng::game {

/// The C++ a project writes in its own `src/` folder (ADR-011): rules the
/// data tables cannot say — when a level is won, what happens at the third
/// minute, how a boss's second phase starts.
///
/// Subclass it, hold whatever state the rules need as members, and hand the
/// class to `SIMPLISH_GAME_LOGIC` (`game-logic-entry.h`) once, in one file:
///
/// @code
///   class Survive final : public eng::game::GameLogic {
///   public:
///     void tick(eng::game::GameLogicWorld& world) override {
///       if (world.tick() == 60 * 90) {
///         world.endRun(eng::game::RunOutcome::WON);
///       }
///     }
///   };
///   SIMPLISH_GAME_LOGIC(Survive)
/// @endcode
///
/// A new instance is made for every run — every playtest, every game — so
/// members start from their initialisers each time and nothing carries
/// over. The logic runs inside the deterministic tick, so it is held to the
/// same rules as the engine's own systems (ADR-002): no clocks, no
/// unordered iteration, no randomness but `GameLogicWorld::random`, and
/// every member that a later tick decides anything by folded into
/// `hashState`.
class GameLogic {
public:
  virtual ~GameLogic() = default;

  /// Once, on tick 0, before the first `tick`: set the run up.
  virtual void start([[maybe_unused]] GameLogicWorld& world) {}

  /// Every tick, in the director's phase: after damage, before the dead
  /// are cleared away.
  virtual void tick(GameLogicWorld& world) = 0;

  /// Fold every member a later tick decides anything by into @p hash, in a
  /// fixed order. A logic with no state of its own needs nothing here.
  virtual void hashState([[maybe_unused]] GameLogicHash& hash) const {}

  /// Once, at the end of the tick the run ended on — won, lost, whoever
  /// ended it — with `world.outcome()` saying how. The last call a run
  /// makes: the tick after is never played, so its writes do nothing, but
  /// what it logs is heard. Its events are the last tick's, not this one's.
  virtual void end([[maybe_unused]] GameLogicWorld& world) {}

  GameLogic(const GameLogic&) = delete;
  GameLogic& operator=(const GameLogic&) = delete;
  GameLogic(GameLogic&&) = delete;
  GameLogic& operator=(GameLogic&&) = delete;

protected:
  GameLogic() = default;
};

}  // namespace eng::game
