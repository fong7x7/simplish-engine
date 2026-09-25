#pragma once

/// @file editor-water-splash.h
/// @brief Where water was splashed, and how hard.
/// @par Threading A value type.

#include <engine/math/vec2.h>

namespace eng::editor {

/// A splash the water owes the effects: where something landed in it or
/// stepped through it, and how big a splash to throw there
/// (`waterSplashEffect`'s scale).
struct EditorWaterSplash {
  /// Where it landed, in tiles.
  Vec2 at{};
  /// How big a splash: 1 for a shot.
  float scale = 1.0f;
};

}  // namespace eng::editor
