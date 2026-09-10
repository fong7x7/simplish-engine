#pragma once

/// @file editor-id-kind.h
/// @brief What sort of thing an id names, which is a reference's prefix.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// The kinds of thing the project format lets one file reference in
/// another.
///
/// References are `kind:id` strings rather than paths
/// ([project-format.md §3]), and the prefix is what lets the content
/// generator type-check a reference without loading its target first. The
/// ones here are what the editor can name today: the two sorts of asset
/// that can be placed, and the three sorts of thing a level holds.
/// @thread_safety Immutable value type.
enum class EditorIdKind : uint8_t {
  /// A model read from the project's assets directory.
  MESH,
  /// One of the editor's built-in shapes.
  SHAPE,
  /// A placed asset, standing in the level.
  PROP,
  /// A light, standing in the level.
  LIGHT,
  /// A point a player enters the level at.
  PLAYER_START,
};

/// The prefix a reference to @p kind carries, without the colon.
[[nodiscard]] constexpr std::string_view editorIdKindPrefix(EditorIdKind kind) {
  switch (kind) {
    case EditorIdKind::MESH:
      return "mesh";
    case EditorIdKind::SHAPE:
      return "shape";
    case EditorIdKind::PROP:
      return "prop";
    case EditorIdKind::LIGHT:
      return "light";
    case EditorIdKind::PLAYER_START:
      return "player_start";
  }
  return {};
}

}  // namespace eng::editor
