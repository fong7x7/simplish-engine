#include <editor/shell/editor-character-figure.h>
#include <editor/shell/editor-entity-id.h>

namespace eng::editor {

std::vector<EditorCharacterFigure>
editorStartFigures(const EditorDocument& document) {
  std::vector<EditorCharacterFigure> figures;
  for (const EditorPlayerStart& start : document.player_starts) {
    if (start.character.empty()) {
      continue;
    }
    const WorldPoint& at = start.position;
    figures.push_back(
        {editorPlayerStartRef(start), start.character, {at.x, at.y, at.z}});
  }
  return figures;
}

}  // namespace eng::editor
