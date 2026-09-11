#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-character-figure.h>
#include <editor/shell/editor-entity-id.h>

namespace eng::editor {

std::vector<EditorCharacterFigure>
editorStartFigures(const EditorDocument& document,
                   const std::vector<game::CharacterDefinition>& characters) {
  std::vector<EditorCharacterFigure> figures;
  for (const EditorPlayerStart& start : document.player_starts) {
    const std::optional<size_t> named =
        findEditorCharacter(characters, start.character);
    if (!named) {
      continue;
    }
    const WorldPoint& at = start.position;
    figures.push_back({editorPlayerStartRef(start),
                       characters[*named].model,
                       {at.x, at.y, at.z}});
  }
  return figures;
}

}  // namespace eng::editor
