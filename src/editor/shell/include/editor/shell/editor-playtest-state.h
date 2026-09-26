#pragma once

/// @file editor-playtest-state.h
/// @brief A playtest, as far as the rest of the editor can see it.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/editor-play-mode.h>
#include <editor/shell/editor-playtest-actor.h>
#include <editor/shell/editor-playtest-clock.h>
#include <editor/shell/editor-playtest-effects.h>
#include <editor/shell/editor-playtest-hazard.h>
#include <editor/shell/editor-playtest-player.h>
#include <editor/shell/editor-playtest-ui.h>
#include <editor/shell/editor-scripted-input.h>
#include <game/logic/run-outcome.h>
#include <game/world/world-cue.h>
#include <optional>
#include <string>
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
  /// Whether a running playtest advances on its own or waits to be stepped.
  EditorPlaytestClock clock = EditorPlaytestClock::RUNNING;
  /// Ticks simulated since the playtest started.
  uint64_t tick = 0;
  /// Ticks the frame clock dropped rather than ran — a frame that took
  /// longer than four ticks' worth. Non-zero means the playtest stuttered.
  uint64_t dropped_ticks = 0;
  /// The combined tick hash at the end of the last tick run, if any has.
  std::optional<uint64_t> hash;
  /// Every player, in the simulation's own order.
  std::vector<EditorPlaytestPlayer> players;
  /// Every actor — every prop with a behavior — in the level's order.
  std::vector<EditorPlaytestActor> actors;
  /// Where every projectile in flight is.
  std::vector<WorldPoint> projectiles;
  /// Every hazard pool on the floor.
  std::vector<EditorPlaytestHazard> hazards;
  /// The effects shots, hits and blasts have played.
  EditorPlaytestEffects effects{};
  /// Whether the run is over: the game logic ended it, or no player is up.
  bool run_over = false;
  /// How the run stands: still playing, won or lost.
  game::RunOutcome outcome = game::RunOutcome::PLAYING;
  /// Whether the playtest runs the project's game logic.
  bool logic = false;
  /// Whether the game logic has the game paused: gameplay stands still,
  /// its screens and input go on. Not the editor's own clock pause.
  bool game_paused = false;
  /// Ticks played unpaused, which gameplay is timed by.
  uint64_t play_tick = 0;
  /// The last lines the game logic said, oldest first; at most
  /// `EDITOR_LOGIC_LOG_LINES`.
  std::vector<std::string> logic_log;
  /// The last cues the game logic raised, oldest first; at most
  /// `EDITOR_LOGIC_CUES`.
  std::vector<game::WorldCue> logic_cues;
  /// The game's own screens shown, and a choice waiting to be made.
  EditorPlaytestUi ui;
  /// Input queued for player 1, oldest first. While any is queued it runs
  /// in place of the keyboard, one tick at a time.
  std::vector<EditorScriptedInput> scripted;
};

}  // namespace eng::editor
