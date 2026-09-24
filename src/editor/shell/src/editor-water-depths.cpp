#include <charconv>
#include <cmath>
#include <cstdio>
#include <editor/shell/editor-water-depths.h>
#include <engine/render-water/water-depth.h>

namespace eng::editor {

namespace {

  /// @p text read as a whole number of tiles, or nothing when it is not
  /// one.
  std::optional<float> tilesFrom(std::string_view text) {
    float value = 0.0f;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() ||
        !std::isfinite(value)) {
      return std::nullopt;
    }
    return value;
  }

  /// @p tiles as the panel writes it: as few decimals as it needs.
  std::string tilesText(float tiles) {
    char text[32];
    (void)std::snprintf(text, sizeof(text), "%g %s", static_cast<double>(tiles),
                        tiles == 1.0f ? "tile" : "tiles");
    return text;
  }

}  // namespace

std::optional<float> editorWaterDepthNamed(std::string_view word) {
  for (const EditorWaterDepth& depth : EDITOR_WATER_DEPTHS) {
    if (depth.word == word) {
      return depth.tiles;
    }
  }
  const std::optional<float> tiles = tilesFrom(word);
  if (!tiles || *tiles < WATER_DEPTH_STEP || *tiles > WATER_MAX_DEPTH) {
    return std::nullopt;
  }
  return tiles;
}

size_t editorNearestWaterDepth(float tiles) {
  size_t nearest = 0;
  float best = INFINITY;
  const float at = std::log(std::max(tiles, WATER_DEPTH_STEP));
  for (size_t i = 0; i < EDITOR_WATER_DEPTH_COUNT; ++i) {
    const float off = std::abs(std::log(EDITOR_WATER_DEPTHS[i].tiles) - at);
    if (off < best) {
      best = off;
      nearest = i;
    }
  }
  return nearest;
}

std::string editorWaterDepthLabel(uint8_t units) {
  const float tiles = waterDepthTiles(units);
  const EditorWaterDepth& named =
      EDITOR_WATER_DEPTHS[editorNearestWaterDepth(tiles)];
  if (waterDepthUnits(named.tiles) == waterDepthUnits(tiles)) {
    return std::string(named.name) + " (" + tilesText(tiles) + ")";
  }
  return tilesText(tiles);
}

}  // namespace eng::editor
