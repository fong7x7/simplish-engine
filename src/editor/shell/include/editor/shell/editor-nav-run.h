#pragma once

/// @file editor-nav-run.h
/// @brief A run of neighbouring navigation cells of one kind along a row.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <editor/shell/editor-nav-cell.h>

namespace eng::editor {

/// Cells `first` to one before `end` of row `row`, all of `kind` — what the
/// navigation overlay draws as one band, so a wall a hundred cells long is
/// one stroke rather than a hundred.
/// @thread_safety Immutable value type.
struct EditorNavRun {
  /// The row, counted along world +Y from the grid's origin.
  uint32_t row = 0;
  /// The first column of the run.
  uint32_t first = 0;
  /// One past the last column of the run.
  uint32_t end = 0;
  /// What every cell of it is.
  EditorNavCell kind = EditorNavCell::OPEN;
};

}  // namespace eng::editor
