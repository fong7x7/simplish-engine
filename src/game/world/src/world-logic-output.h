#pragma once

/// @file world-logic-output.h
/// @brief Where what game logic says and shows is kept for presentation.
/// @par Threading
/// A view over a world's own lists, for one call of the logic.

#include <game/world/world-cue.h>
#include <game/world/world-ui.h>
#include <span>
#include <string>
#include <vector>

namespace eng::game {

/// What of a logic's call is for whoever presents the game, never for the
/// tick: its log lines, its cues, and its screens.
struct WorldLogicOutput {
  /// Where what the logic says is kept for whoever runs the game.
  std::vector<std::string>& log;
  /// Where the cues the logic raises are kept for whoever presents it.
  std::vector<WorldCue>& cues;
  /// The screens the logic shows and the values they show.
  WorldUi& ui;
  /// The project's screens, by id: those the logic may show.
  std::span<const std::string> screens;
};

}  // namespace eng::game
