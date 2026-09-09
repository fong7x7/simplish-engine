#pragma once

/// @file editor-placement-bounds.h
/// @brief World-space bounding box of a placed asset.
/// @par Threading Thread-safe (immutable value type).

#include <engine/math/vec3.h>

namespace eng::editor {

/// Axis-aligned box a placement occupies in world tile units.
///
/// Computed from the asset's own bounds through the placement transform, so
/// it follows a translation and grows to contain a rotation. It is what the
/// viewport outlines and what a click is tested against — neither needs the
/// mesh itself, which the editor does not keep on the CPU.
/// @thread_safety Immutable value type.
struct PlacementBounds {
  /// Minimum corner, in world tile units.
  Vec3 min{};
  /// Maximum corner, in world tile units.
  Vec3 max{};
};

}  // namespace eng::editor
