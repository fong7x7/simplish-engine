#include <editor/shell/editor-terrains.h>

namespace eng::editor {

const EditorTerrain* editorTerrainAt(uint8_t terrain) {
  if (terrain == 0 || terrain > EDITOR_TERRAIN_COUNT) {
    return nullptr;
  }
  return &EDITOR_TERRAINS[terrain - 1];
}

std::string_view editorTerrainWord(uint8_t terrain) {
  if (terrain == 0) {
    return EDITOR_BARE_GROUND_WORD;
  }
  const EditorTerrain* found = editorTerrainAt(terrain);
  return found != nullptr ? found->word : std::string_view{};
}

std::optional<uint8_t> editorTerrainNamed(std::string_view word) {
  if (word.starts_with(EDITOR_TILE_REF_PREFIX)) {
    word.remove_prefix(EDITOR_TILE_REF_PREFIX.size());
  }
  if (word == EDITOR_BARE_GROUND_WORD) {
    return uint8_t{0};
  }
  for (size_t i = 0; i < EDITOR_TERRAIN_COUNT; ++i) {
    if (EDITOR_TERRAINS[i].word == word) {
      return static_cast<uint8_t>(i + 1);
    }
  }
  return std::nullopt;
}

}  // namespace eng::editor
