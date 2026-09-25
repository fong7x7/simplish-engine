#pragma once

/// @file editor-playtest-run.h
/// @brief Which level a playtest is of, and the game logic it runs.
/// @par Threading Main-thread-only.

#include <editor/build/editor-logic-library.h>
#include <memory>
#include <string>

namespace eng::editor {

/// What a playtest is, besides its setup and its content.
/// @thread_safety Main-thread-only.
struct EditorPlaytestRun {
  /// The level being played: what its replay is recorded as.
  std::string level_id;
  /// The project's game logic, loaded; null to play without. Shared, so
  /// the library stays open for as long as the playtest's instance of the
  /// logic — code inside it — lives, even when a rebuild has loaded a
  /// newer one for the next playtest.
  std::shared_ptr<EditorLogicLibrary> logic{};
};

}  // namespace eng::editor
