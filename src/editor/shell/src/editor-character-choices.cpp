#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>

namespace eng::editor {

namespace {

  /// What a character reference starts with.
  constexpr std::string_view CHARACTER_PREFIX = "character:";

}  // namespace

EditorCharacterChoices
editorCharacterChoices(const std::vector<game::CharacterDefinition>& characters,
                       const std::string& character) {
  EditorCharacterChoices choices;
  choices.names.emplace_back(EDITOR_CHARACTER_NONE_NAME);
  choices.refs.emplace_back();
  for (const game::CharacterDefinition& each : characters) {
    choices.names.push_back(each.name);
    choices.refs.push_back(editorCharacterRef(each.id));
  }
  if (const std::optional<size_t> found =
          findEditorCharacter(characters, character)) {
    choices.current = *found + 1U;
  } else if (!character.empty()) {
    choices.names.push_back(character + " (missing)");
    choices.refs.push_back(character);
    choices.current = choices.refs.size() - 1U;
  }
  return choices;
}

std::string editorCharacterRef(std::string_view id) {
  return editorQualifiedId(EditorIdKind::CHARACTER, id);
}

std::string editorCharacterIdOf(std::string_view ref) {
  return ref.starts_with(CHARACTER_PREFIX)
             ? std::string(ref.substr(CHARACTER_PREFIX.size()))
             : std::string{};
}

std::optional<size_t>
findEditorCharacter(const std::vector<game::CharacterDefinition>& characters,
                    std::string_view ref) {
  const std::string id = editorCharacterIdOf(ref);
  if (id.empty()) {
    return std::nullopt;
  }
  for (size_t i = 0; i < characters.size(); ++i) {
    if (characters[i].id == id) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<size_t>
findEditorAssetByRef(const std::vector<EditorAsset>& assets,
                     std::string_view ref) {
  if (ref.empty()) {
    return std::nullopt;
  }
  for (size_t i = 0; i < assets.size(); ++i) {
    if (editorAssetRef(assets[i]) == ref) {
      return i;
    }
  }
  return std::nullopt;
}

}  // namespace eng::editor
