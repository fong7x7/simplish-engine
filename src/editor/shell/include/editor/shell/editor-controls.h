#pragma once

/// @file editor-controls.h
/// @brief The user's control scheme, as the shell holds it.
/// @par Threading Main-thread only.

#include <cstdint>
#include <editor/shell/editor-playtest-controls.h>
#include <engine/input/input-bindings.h>
#include <filesystem>

namespace eng::editor {

/// The keys and pad controls the playtest reads, where they are kept, and a
/// count of changes, so whatever changes them — the Controls screen, an
/// agent's `set_controls` — only has to bump `revision` for the editor to
/// save them.
struct EditorControls {
  /// Which controls ask for which action.
  input::InputBindings bindings = editorDefaultInputBindings();
  /// The file they are kept in; empty when there is nowhere to keep them.
  std::filesystem::path file;
  /// Bumped on every change; the editor saves when it moves on from the
  /// revision it last wrote.
  uint64_t revision = 0;
};

}  // namespace eng::editor
