#pragma once

/// @file editor-playtest-state.h
/// @brief A playtest, as far as the rest of the editor can see it.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/editor-play-mode.h>
#include <editor/shell/editor-playtest-player.h>
#include <editor/shell/editor-scripted-input.h>
#include <optional>
#include <vector>

namespace eng::editor {

/// Whether a playtest is running, and what it has done so far.
///
/// A mirror, refreshed by the editor after every frame of play, rather than
/// the running game itself: the agent API is a pure function of
/// `EditorShellState`, and this is how it reads a playtest without holding
/// the simulation. The one thing that flows the other way is `scripted`,
/// which the agent fills and the playtest drains.
/// @thread_safety Main-thread-only.
struct EditorPlaytestState {
  /// Editing, or playing.
  EditorPlayMode mode = EditorPlayMode::EDITING;
  /// Ticks simulated since the playtest started.
  uint64_t tick = 0;
  /// Ticks the frame clock dropped rather than ran — a frame that took
  /// longer than four ticks' worth. Non-zero means the playtest stuttered.
  uint64_t dropped_ticks = 0;
  /// The combined tick hash at the end of the last tick run, if any has.
  std::optional<uint64_t> hash;
  /// Every player, in the simulation's own order.
  std::vector<EditorPlaytestPlayer> players;
  /// Input queued for player 1, oldest first. While any is queued it runs
  /// in place of the keyboard, one tick at a time.
  std::vector<EditorScriptedInput> scripted;
};

}  // namespace eng::editor
