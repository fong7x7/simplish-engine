#pragma once

/// @file editor-water-depth-choices.h
/// @brief What the properties panel's Depth row offers for an area of
/// water.
/// @par Threading Thread-safe (pure function over value types).

#include <cstddef>
#include <engine/render-ground/ground-cell.h>
#include <engine/render-ground/ground-grid.h>
#include <span>
#include <string>
#include <vector>

namespace eng::editor {

/// The Depth row: every named depth, and which one the area is at.
///
/// An area at a depth no name has — set by number through the agent API —
/// or at more than one depth gets one more row after the names saying so,
/// and that is the current one; choosing it changes nothing.
/// @thread_safety Immutable value type.
struct EditorWaterDepthChoices {
  /// Each row's text: every named depth with its tiles, then perhaps one
  /// for the depth the area is at.
  std::vector<std::string> names;
  /// Which row the area is at now.
  size_t current = 0;
};

/// The Depth row for the water at @p cells, as deep as @p depths says.
[[nodiscard]] EditorWaterDepthChoices
editorWaterDepthChoices(const GroundGrid& depths,
                        std::span<const GroundCell> cells);

}  // namespace eng::editor
