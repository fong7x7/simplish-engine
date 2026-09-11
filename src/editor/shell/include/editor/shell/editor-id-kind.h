#pragma once

/// @file editor-id-kind.h
/// @brief What sort of thing an id names, which is a reference's prefix.
/// @par Threading Thread-safe (immutable value type).

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

namespace eng::editor {

/// The kinds of thing the project format lets one file reference in
/// another.
///
/// References are `kind:id` strings rather than paths
/// ([project-format.md §3]), and the prefix is what lets the content
/// generator type-check a reference without loading its target first. The
/// ones here are what the editor can name today: the two sorts of asset
/// that can be placed, the three sorts of thing a level holds, and a row of
/// the characters or the behaviors table.
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
  /// A character a player can play as, from the characters table.
  CHARACTER,
  /// A behavior an actor runs: a built-in one, or a row of the behaviors
  /// table.
  BEHAVIOR,
  /// A point of a patrol route, standing in the level.
  WAYPOINT,
};

/// Every kind's prefix, without the colon, in enumerator order.
inline constexpr std::string_view EDITOR_ID_KIND_PREFIXES[] = {
    "mesh",         "shape",     "prop",     "light",
    "player_start", "character", "behavior", "waypoint",
};

static_assert(std::size(EDITOR_ID_KIND_PREFIXES) ==
                  static_cast<size_t>(EditorIdKind::WAYPOINT) + 1,
              "every id kind needs a prefix");

/// The prefix a reference to @p kind carries, without the colon.
[[nodiscard]] constexpr std::string_view editorIdKindPrefix(EditorIdKind kind) {
  return EDITOR_ID_KIND_PREFIXES[static_cast<size_t>(kind)];
}

}  // namespace eng::editor
