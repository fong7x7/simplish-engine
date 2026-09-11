#pragma once

/// @file editor-character-choices.h
/// @brief What a player start's Character row offers, and which asset a
/// start's character is.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-asset.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// What the Character row calls a start with no character of its own.
inline constexpr std::string_view EDITOR_CHARACTER_STAND_IN_NAME = "Stand-in";

/// The characters a player start can be given, in the order the Character
/// row steps through them, and which of them it has now.
/// @thread_safety Immutable value type.
struct EditorCharacterChoices {
  /// What the row shows for each choice: the stand-in first, then every
  /// asset by name, in the browser's order.
  std::vector<std::string> names{};
  /// The reference each choice writes into the start, alongside `names`:
  /// empty for the stand-in, the asset's qualified reference otherwise.
  std::vector<std::string> refs{};
  /// Which choice the start has now.
  size_t current = 0;
};

/// Every character in @p assets, with @p character — a start's reference —
/// picked.
///
/// Every asset is offered, the built-in shapes included: a cube is a
/// perfectly good player while the real model is being made. A reference
/// naming no asset in the list is offered too, last, as `<ref> (missing)`,
/// so the row says what the start holds rather than pretending it is the
/// stand-in; stepping off it is the way to replace it.
[[nodiscard]] EditorCharacterChoices
editorCharacterChoices(const std::vector<EditorAsset>& assets,
                       const std::string& character);

/// Index of the asset @p character references in @p assets, or nothing for
/// the stand-in and for an asset the list does not hold.
[[nodiscard]] std::optional<size_t>
findEditorCharacterAsset(const std::vector<EditorAsset>& assets,
                         const std::string& character);

}  // namespace eng::editor
