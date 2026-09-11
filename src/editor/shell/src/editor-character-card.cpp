#include <cstdio>
#include <editor/shell/editor-character-card.h>
#include <editor/shell/editor-character-choices.h>

namespace eng::editor {

namespace {

  /// @p speed to one decimal place: `Speed 6.5`.
  std::string speedLine(float speed) {
    char text[32] = {};
    (void)std::snprintf(text, sizeof(text), "Speed %.1f",
                        static_cast<double>(speed));
    return text;
  }

  /// The thumbnail of the asset @p model references, when it has one.
  RhiTextureHandle pictureOf(const std::vector<EditorAsset>& assets,
                             const std::string& model) {
    const std::optional<size_t> asset = findEditorAssetByRef(assets, model);
    if (!asset ||
        assets[*asset].thumbnail_state != EditorAssetThumbnailState::READY) {
      return RHI_TEXTURE_INVALID;
    }
    return assets[*asset].thumbnail;
  }

}  // namespace

std::vector<EditorCharacterCard> makeEditorCharacterCards(
    const std::vector<game::CharacterDefinition>& characters,
    const std::vector<EditorAsset>& assets) {
  std::vector<EditorCharacterCard> cards;
  cards.reserve(characters.size());
  for (const game::CharacterDefinition& character : characters) {
    cards.push_back({character.name, speedLine(character.move_speed),
                     "Health " + std::to_string(character.health),
                     pictureOf(assets, character.model)});
  }
  return cards;
}

}  // namespace eng::editor
