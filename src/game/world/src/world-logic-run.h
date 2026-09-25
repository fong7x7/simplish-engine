#pragma once

/// @file world-logic-run.h
/// @brief What of the run itself game logic may set.
/// @par Threading
/// A view over a world's own fields, for one call of the logic.

#include <game/logic/logic-steps.h>
#include <game/logic/run-outcome.h>

namespace eng::game {

/// The run's own settings the logic writes straight through, rather than
/// by a queued command: both are read only by later phases or ticks.
struct WorldLogicRun {
  /// How the logic has ended the run; `PLAYING` until it does.
  RunOutcome& outcome;
  /// Whose steps the logic hears.
  LogicSteps& steps;
};

}  // namespace eng::game
