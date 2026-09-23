#pragma once

/// @file editor-shape-kind.h
/// @brief The simple geometry the browser offers without a file behind it.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace eng::editor {

/// One of the built-in shapes.
///
/// These are placed exactly as a scanned model is — they are assets whose
/// geometry is generated rather than read — so this says only which shape
/// to generate. It lives in a header of its own because `editor-asset.h`
/// names it and `editor-shape.h` needs an asset back.
/// @thread_safety Immutable value type.
enum class EditorShapeKind : uint8_t {
  /// A box, one tile on every side.
  CUBE,
  /// A cylinder standing on its flat end.
  CYLINDER,
  /// A square pyramid.
  PYRAMID,
  /// A sphere resting on the ground plane.
  SPHERE,
  /// A thin slab lying on the ground, one tile across: the stand-in for
  /// anything flat — a floor tile, a rug, a hole, a patch of spilt oil.
  TILE,
};

/// Every built-in shape, in the order the shapes folder lists them: by
/// name, as every other folder in the browser is sorted.
inline constexpr EditorShapeKind EDITOR_SHAPE_KINDS[] = {
    EditorShapeKind::CUBE,    EditorShapeKind::CYLINDER,
    EditorShapeKind::PYRAMID, EditorShapeKind::SPHERE,
    EditorShapeKind::TILE,
};

/// How many built-in shapes there are.
inline constexpr size_t EDITOR_SHAPE_COUNT =
    sizeof(EDITOR_SHAPE_KINDS) / sizeof(EDITOR_SHAPE_KINDS[0]);

/// Name shown on the shape's card, and on the properties panel's line for
/// a placement of one.
[[nodiscard]] constexpr std::string_view editorShapeName(EditorShapeKind kind) {
  switch (kind) {
    case EditorShapeKind::CUBE:
      return "Cube";
    case EditorShapeKind::CYLINDER:
      return "Cylinder";
    case EditorShapeKind::PYRAMID:
      return "Pyramid";
    case EditorShapeKind::SPHERE:
      return "Sphere";
    case EditorShapeKind::TILE:
      return "Tile";
  }
  return {};
}

/// Whether a shape is solid when it is first dropped: what a new
/// placement's `collides` starts as.
///
/// Every shape is but the tile. A tile is laid on the floor to be walked
/// over, so starting it solid would have the playtest stop a player at the
/// edge of every rug; a hole that should block is the rarer case, and is
/// one tick in the properties panel.
[[nodiscard]] constexpr bool editorShapeCollides(EditorShapeKind kind) {
  return kind != EditorShapeKind::TILE;
}

}  // namespace eng::editor
