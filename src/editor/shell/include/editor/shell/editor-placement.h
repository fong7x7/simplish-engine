#pragma once

/// @file editor-placement.h
/// @brief One placed instance of an asset in the level.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec3.h>
#include <string>

namespace eng::editor {

/// An asset placed at a world position.
///
/// Saved to the project's level file and read back when the project is
/// opened (`editor-level-io.h`), which is what makes the asset panel an
/// authoring tool rather than only a placement one.
/// @thread_safety Main-thread-only.
struct EditorPlacement {
  /// Stable identifier for this one placed thing: `crate_01`. Assigned
  /// when it is placed and never reused, so a logic file can say
  /// `prop:crate_01` and mean this crate rather than whatever currently
  /// sits at some position in a list — see `editor-entity-id.h`.
  std::string id{};
  /// Index into the shell's asset list. A handle for this session only:
  /// what survives a rescan is the asset's own id, which is what the
  /// index is rebound through.
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
  /// Whether a player can walk through it. Solid by default: most of what
  /// is dropped into a level is a crate or a wall, and the few that are not
  /// — grass, a rug, a decal — are ticked off in the properties panel.
  bool collides = true;
};

}  // namespace eng::editor
