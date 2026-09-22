#pragma once

/// @file editor-sprite.h
/// @brief One sprite billboard placed in the level.
/// @par Threading Main-thread-only.

#include <editor/shell/iso-projection.h>
#include <engine/render-sprite/sprite-sheet.h>
#include <string>

namespace eng::editor {

/// A sprite billboard: an upright quad standing at a point in the level,
/// facing the camera, showing one frame of a sprite sheet at a time.
///
/// The sprite half of
/// [ADR-003](../../../../../docs/decisions/ADR-003-hybrid-iso-render-model.md):
/// 2D art in a 3D scene, drawn in the same depth-buffered pass as the
/// meshes, so what is behind it hides it and what is in front of it does
/// not. Its empty texels do not draw at all — the mesh fragment stage cuts
/// them out at `MESH_ALPHA_CUTOFF` — which is what lets it write depth like
/// any other mesh instead of being composited over the scene.
///
/// It names its sheet rather than holding it: the image is a file under the
/// project's assets, shared by every billboard that shows it, and a level
/// file has to survive that file being re-exported. Saved with the level as
/// an entity of definition `entity:sprite_billboard`
/// ([project-format.md §4.1]).
///
/// Presentation only. A billboard is scenery the simulation never sees: it
/// stops nobody, and nothing in a tick reads it.
/// @thread_safety Main-thread-only.
struct EditorSprite {
  /// Stable identifier for this one billboard: `sprite_01`. Assigned when
  /// it is added and never reused — see `editor-entity-id.h`.
  std::string id{};
  /// The sheet image it shows, as a path relative to the project's assets
  /// directory: `sprites/slime.png`. Empty until one is picked, which
  /// draws nothing.
  ///
  /// The path rather than an asset index: sheets are not placeable assets
  /// and carry no index, and the path is what survives a rescan and reads
  /// in a hand-edited file.
  std::string sheet{};
  /// Where its base stands: the middle of the tile it was dropped on, on
  /// the floor, which is the point its depth is measured at.
  WorldPoint position{};
  /// How its sheet is cut into frames, and how fast they play.
  SpriteSheet grid{};
  /// How tall it stands, in tiles. Its width follows from this and the
  /// shape of one frame, so a sheet is never stretched — see
  /// `editor-sprite-transform.h`.
  float height = 1.0f;
};

}  // namespace eng::editor
