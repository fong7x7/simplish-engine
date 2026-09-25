#pragma once

/// @file pause.h
/// @brief Pausing the game, with a screen to show while it is.
/// @par Threading
/// Main-thread-only; the logic's own thread, inside its tick.

#include <game/logic/game-logic-world.h>
#include <string_view>

namespace eng::game::sdk {

/// Pause the game and show the screen @p screen, or — paused already —
/// hide it and play on; either from the next tick. Whether the game is
/// now to be paused. An empty @p screen shows and hides nothing.
///
/// @code
///   void onPausePressed(GameLogicWorld& world, const LogicEvent&) override {
///     sdk::togglePause(world, "pause");
///   }
///   void onUiAction(GameLogicWorld& world, const LogicEvent& e) override {
///     if (sdk::chose(e, "resume")) sdk::togglePause(world, "pause");
///   }
/// @endcode
bool togglePause(GameLogicWorld& world, std::string_view screen);

}  // namespace eng::game::sdk
