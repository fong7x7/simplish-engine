#pragma once

/// @file editor-ground-brush.h
/// @brief What the Tile tool's brush lays down.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// The three things the Tile tool paints: a terrain on the ground, water
/// over the ground, or the water taken off it again. Water is its own
/// layer, so painting it leaves the terrain under it as it was, and drying
/// it leaves that terrain showing.
/// @thread_safety Immutable value type.
enum class EditorGroundBrush : uint8_t {
  /// A terrain, or bare ground to erase one.
  TERRAIN,
  /// Water, at the brush's depth.
  WATER,
  /// No water: dries whatever water the brush passes over.
  DRY,
};

}  // namespace eng::editor
