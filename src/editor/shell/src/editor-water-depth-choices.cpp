#include <editor/shell/editor-water-depth-choices.h>
#include <editor/shell/editor-water-depths.h>
#include <engine/render-water/water-depth.h>
#include <optional>

namespace eng::editor {

namespace {

  /// The one depth every cell of @p cells is at, in steps with the default
  /// written out, or nothing when they differ.
  std::optional<uint8_t> sharedDepth(const GroundGrid& depths,
                                     std::span<const GroundCell> cells) {
    std::optional<uint8_t> shared;
    for (const GroundCell cell : cells) {
      const uint8_t units = waterDepthUnits(waterDepthTiles(depths.at(cell)));
      if (shared && *shared != units) {
        return std::nullopt;
      }
      shared = units;
    }
    return shared;
  }

  /// The named depth @p units is exactly, or nothing.
  std::optional<size_t> namedIndex(uint8_t units) {
    for (size_t i = 0; i < EDITOR_WATER_DEPTH_COUNT; ++i) {
      if (waterDepthUnits(EDITOR_WATER_DEPTHS[i].tiles) == units) {
        return i;
      }
    }
    return std::nullopt;
  }

}  // namespace

EditorWaterDepthChoices
editorWaterDepthChoices(const GroundGrid& depths,
                        std::span<const GroundCell> cells) {
  EditorWaterDepthChoices choices;
  for (const EditorWaterDepth& depth : EDITOR_WATER_DEPTHS) {
    choices.names.push_back(
        editorWaterDepthLabel(waterDepthUnits(depth.tiles)));
  }
  const std::optional<uint8_t> shared = sharedDepth(depths, cells);
  const std::optional<size_t> named =
      shared ? namedIndex(*shared) : std::nullopt;
  if (named) {
    choices.current = *named;
    return choices;
  }
  choices.names.push_back(shared ? editorWaterDepthLabel(*shared)
                                 : std::string("Mixed depths"));
  choices.current = EDITOR_WATER_DEPTH_COUNT;
  return choices;
}

}  // namespace eng::editor
