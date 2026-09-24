#pragma once

/// @file editor-water-depth.h
/// @brief One named depth water can be painted at.
/// @par Threading Thread-safe (immutable value type).

#include <string_view>

namespace eng::editor {

/// A depth by name: what the brush and the properties panel offer, and the
/// word the agent API takes for it.
/// @thread_safety Immutable value type.
struct EditorWaterDepth {
  /// What the brush and the panel call it: `Puddle`.
  std::string_view name;
  /// The word the agent API names it by: `puddle`.
  std::string_view word;
  /// How deep it is, in tiles.
  float tiles = 0.0f;
};

}  // namespace eng::editor
