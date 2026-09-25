#pragma once

/// @file game-logic-factory.h
/// @brief The pair of functions that make and unmake a project's logic.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/logic/game-logic.h>

namespace eng::game {

/// Makes one instance of a project's logic; what `SIMPLISH_GAME_LOGIC`
/// exports as `simplishCreateGameLogic`.
using GameLogicCreateFn = GameLogic* (*)();
/// Unmakes an instance the matching create made.
using GameLogicDestroyFn = void (*)(GameLogic*);
/// The logic API version a module was built against.
using GameLogicApiVersionFn = uint32_t (*)();

/// Where a project's logic comes from: the two functions its module
/// exports, resolved from a shared library in the editor or linked in
/// directly in a deployed game. An instance must be unmade by the destroy
/// of the same module that made it, since only that module's code knows
/// how — which is why the two travel together.
///
/// Empty — both null — for a project with no logic, which runs the game's
/// own rules alone.
struct GameLogicFactory {
  /// Makes an instance.
  GameLogicCreateFn create = nullptr;
  /// Unmakes one.
  GameLogicDestroyFn destroy = nullptr;
};

}  // namespace eng::game
