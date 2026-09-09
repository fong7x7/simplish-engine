#pragma once

/// @file editor-placement.h
/// @brief One placed instance of an asset in the level.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec3.h>

namespace eng::editor {

/// An asset placed at a world position.
///
/// Placements live in memory only. Persisting them needs the level format
/// ([Editor REQUIREMENTS §4.4]), so closing the editor loses them — which
/// is why the asset panel is a placement tool today rather than an
/// authoring one.
/// @thread_safety Main-thread-only.
struct EditorPlacement {
  /// Index into the shell's asset list.
  size_t asset = 0;
  /// World position of the placement's base.
  WorldPoint position{};
  /// Rotation about the placement's own origin, in degrees.
  ///
  /// Euler angles rather than a quaternion: this is what the properties
  /// panel shows and what a designer types, and the level format will
  /// serialise the same three numbers. The conversion to a matrix is one
  /// place (`makePlacementTransform`), which is where the axis order is
  /// defined.
  Vec3 rotation{};
};

}  // namespace eng::editor
