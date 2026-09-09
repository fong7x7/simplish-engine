#pragma once

/// @file editor-general-item.h
/// @brief The built-in things the browser offers beside a project's files.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-light.h>
#include <string_view>

namespace eng::editor {

/// One item the browser's General section lists.
///
/// These are not files: nothing on disk backs them, and every project has
/// the same ones. They are listed beside the project's assets because that
/// is where somebody looks for something to drop into the level, and
/// dropping one is the same gesture as dropping a model.
/// @thread_safety Immutable value type.
enum class EditorGeneralItem : uint8_t {
  /// A light with parallel rays, standing in for the sun.
  DIRECTIONAL_LIGHT,
  /// A light shining from one point, falling off with distance.
  POINT_LIGHT,
};

/// Every built-in item, in the order the section lists them.
inline constexpr EditorGeneralItem EDITOR_GENERAL_ITEMS[] = {
    EditorGeneralItem::DIRECTIONAL_LIGHT,
    EditorGeneralItem::POINT_LIGHT,
};

/// How many built-in items there are.
inline constexpr size_t EDITOR_GENERAL_ITEM_COUNT =
    sizeof(EDITOR_GENERAL_ITEMS) / sizeof(EDITOR_GENERAL_ITEMS[0]);

/// The kind of light an item drops. Every item is a light today; when one
/// is not, this becomes a question the caller has to ask first.
[[nodiscard]] constexpr EditorLightKind
editorGeneralItemLightKind(EditorGeneralItem item) {
  return item == EditorGeneralItem::DIRECTIONAL_LIGHT
             ? EditorLightKind::DIRECTIONAL
             : EditorLightKind::POINT;
}

/// Name shown on the item's card.
[[nodiscard]] constexpr std::string_view
editorGeneralItemName(EditorGeneralItem item) {
  return item == EditorGeneralItem::DIRECTIONAL_LIGHT ? "Directional Light"
                                                      : "Point Light";
}

}  // namespace eng::editor
