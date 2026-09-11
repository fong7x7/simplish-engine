#pragma once

/// @file editor-character-choices.h
/// @brief What a player start's Character row offers, and turning a start's
/// reference into a character and a character into the asset it is drawn
/// as.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-asset.h>
#include <game/content/character-definition.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// What the Character row calls a start that names no character: its
/// player picks in the selector, which opens on the first character.
inline constexpr std::string_view EDITOR_CHARACTER_NONE_NAME = "None";

/// The characters a player start can name, in the order the Character row
/// steps through them, and which of them it names now.
/// @thread_safety Immutable value type.
struct EditorCharacterChoices {
  /// What the row shows for each choice: none first, then every character
  /// by name, in the table's order.
  std::vector<std::string> names{};
  /// The reference each choice writes into the start, alongside `names`:
  /// empty for none, `character:<id>` otherwise.
  std::vector<std::string> refs{};
  /// Which choice the start has now.
  size_t current = 0;
};

/// Every character in @p characters, with @p character — a start's
/// reference — picked.
///
/// A reference naming no character in the table is offered too, last, as
/// `<ref> (missing)`, so the row says what the start holds rather than
/// pretending it holds nothing; stepping off it is the way to replace it.
[[nodiscard]] EditorCharacterChoices
editorCharacterChoices(const std::vector<game::CharacterDefinition>& characters,
                       const std::string& character);

/// How a start references the character @p id: `character:scout`.
[[nodiscard]] std::string editorCharacterRef(std::string_view id);

/// The character id a start's reference @p ref names — `scout` for
/// `character:scout` — or empty when it is empty or references anything
/// but a character.
[[nodiscard]] std::string editorCharacterIdOf(std::string_view ref);

/// Index of the character @p ref references in @p characters, or nothing
/// when it is empty or names none of them.
[[nodiscard]] std::optional<size_t>
findEditorCharacter(const std::vector<game::CharacterDefinition>& characters,
                    std::string_view ref);

/// Index of the asset whose reference is @p ref in @p assets, or nothing
/// for an empty one and for an asset the list does not hold.
[[nodiscard]] std::optional<size_t>
findEditorAssetByRef(const std::vector<EditorAsset>& assets,
                     std::string_view ref);

}  // namespace eng::editor
