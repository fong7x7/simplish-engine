#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>

namespace eng::editor {

EditorCharacterChoices
editorCharacterChoices(const std::vector<EditorAsset>& assets,
                       const std::string& character) {
  EditorCharacterChoices choices;
  choices.names.emplace_back(EDITOR_CHARACTER_STAND_IN_NAME);
  choices.refs.emplace_back();
  for (const EditorAsset& asset : assets) {
    choices.names.push_back(asset.name);
    choices.refs.push_back(editorAssetRef(asset));
  }
  if (const std::optional<size_t> found =
          findEditorCharacterAsset(assets, character)) {
    choices.current = *found + 1U;
  } else if (!character.empty()) {
    choices.names.push_back(character + " (missing)");
    choices.refs.push_back(character);
    choices.current = choices.refs.size() - 1U;
  }
  return choices;
}

std::optional<size_t>
findEditorCharacterAsset(const std::vector<EditorAsset>& assets,
                         const std::string& character) {
  if (character.empty()) {
    return std::nullopt;
  }
  for (size_t i = 0; i < assets.size(); ++i) {
    if (editorAssetRef(assets[i]) == character) {
      return i;
    }
  }
  return std::nullopt;
}

}  // namespace eng::editor
