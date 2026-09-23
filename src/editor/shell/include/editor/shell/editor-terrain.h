#pragma once

/// @file editor-terrain.h
/// @brief One kind of ground the editor paints with.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// A terrain: what a painted cell is, and what it looks like.
///
/// Built into the editor for now, as the shapes are, so every project can
/// paint a floor before it has any art. Where it sits in
/// `EDITOR_TERRAINS` is its number in the ground grid and so how it
/// stacks: a later terrain is drawn over an earlier one where they meet.
/// @thread_safety Immutable value type.
struct EditorTerrain {
  /// Name shown on its card and in the status line: `Sand`.
  std::string_view name;
  /// The word a level file and the agent API name it by: `sand`, written
  /// in a file as `tile:sand`.
  std::string_view word;
  /// Red of its base colour, sRGB.
  uint8_t red = 0;
  /// Green of its base colour, sRGB.
  uint8_t green = 0;
  /// Blue of its base colour, sRGB.
  uint8_t blue = 0;
  /// How far, in sRGB steps either way, each texel of its swatch strays
  /// from the base colour: the speckle that makes sand read as sand rather
  /// than as a flat fill.
  uint8_t grain = 0;
};

}  // namespace eng::editor
