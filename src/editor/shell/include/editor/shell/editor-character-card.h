#pragma once

/// @file editor-character-card.h
/// @brief One character as the selector shows it.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-asset.h>
#include <engine/render/rhi-core-types.h>
#include <game/content/character-definition.h>
#include <string>
#include <vector>

namespace eng::editor {

/// What the character selector draws for one character: its name, its
/// stats as a player reads them, and a picture of its model.
/// @thread_safety Main-thread-only.
struct EditorCharacterCard {
  /// The character's name.
  std::string name{};
  /// Its speed, as the card prints it: `Speed 6.5`.
  std::string speed{};
  /// Its health, as the card prints it: `Health 4`.
  std::string health{};
  /// The model's thumbnail, or `RHI_TEXTURE_INVALID` when it has none yet —
  /// no model, a model the project lacks, or a picture not made yet.
  RhiTextureHandle picture = RHI_TEXTURE_INVALID;
};

/// A card for each of @p characters, in order, with pictures taken from
/// the thumbnails @p assets already has — the ones the browser made.
[[nodiscard]] std::vector<EditorCharacterCard> makeEditorCharacterCards(
    const std::vector<game::CharacterDefinition>& characters,
    const std::vector<EditorAsset>& assets);

}  // namespace eng::editor
