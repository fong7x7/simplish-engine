#pragma once

/// @file editor-general-item.h
/// @brief The built-in things the browser offers beside a project's files.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-light.h>
#include <optional>
#include <string_view>

namespace eng::editor {

/// One item the browser's general section lists.
///
/// These are not files: nothing on disk backs them, and every project has
/// the same ones. They are listed above the project's assets because that
/// is where somebody looks for something to drop into the level, and
/// dropping one is the same gesture as dropping a model.
/// @thread_safety Immutable value type.
enum class EditorGeneralItem : uint8_t {
  /// A light with parallel rays, standing in for the sun.
  DIRECTIONAL_LIGHT,
  /// A light shining from one point, falling off with distance.
  POINT_LIGHT,
  /// The point a player enters the level at.
  PLAYER_START,
  /// A point of a patrol route.
  WAYPOINT,
};

/// Every built-in item, in the order the section numbers them: the lights,
/// then the tools. Each subsection holds a run of this list, so the order
/// is also the grouping.
inline constexpr EditorGeneralItem EDITOR_GENERAL_ITEMS[] = {
    EditorGeneralItem::DIRECTIONAL_LIGHT,
    EditorGeneralItem::POINT_LIGHT,
    EditorGeneralItem::PLAYER_START,
    EditorGeneralItem::WAYPOINT,
};

/// How many built-in items there are.
inline constexpr size_t EDITOR_GENERAL_ITEM_COUNT =
    sizeof(EDITOR_GENERAL_ITEMS) / sizeof(EDITOR_GENERAL_ITEMS[0]);

/// How many of those, from the front, are light sources: the run the
/// lighting subsection holds.
inline constexpr size_t EDITOR_GENERAL_LIGHT_COUNT = 2;

/// How many follow the lights as tools — things that mark the level for
/// the game rather than showing in it: the run the tools subsection holds.
inline constexpr size_t EDITOR_GENERAL_TOOL_COUNT =
    EDITOR_GENERAL_ITEM_COUNT - EDITOR_GENERAL_LIGHT_COUNT;

/// The kind of light an item drops, or nothing for an item that is not a
/// light — which is the question a drop has to ask first.
[[nodiscard]] constexpr std::optional<EditorLightKind>
editorGeneralItemLightKind(EditorGeneralItem item) {
  switch (item) {
    case EditorGeneralItem::DIRECTIONAL_LIGHT:
      return EditorLightKind::DIRECTIONAL;
    case EditorGeneralItem::POINT_LIGHT:
      return EditorLightKind::POINT;
    case EditorGeneralItem::PLAYER_START:
    case EditorGeneralItem::WAYPOINT:
      return std::nullopt;
  }
  return std::nullopt;
}

/// Name shown on the item's card.
[[nodiscard]] constexpr std::string_view
editorGeneralItemName(EditorGeneralItem item) {
  switch (item) {
    case EditorGeneralItem::DIRECTIONAL_LIGHT:
      return "Directional Light";
    case EditorGeneralItem::POINT_LIGHT:
      return "Point Light";
    case EditorGeneralItem::PLAYER_START:
      return "Player Start";
    case EditorGeneralItem::WAYPOINT:
      return "Waypoint";
  }
  return {};
}

}  // namespace eng::editor
