#include "editor-ground-json.h"

#include <cstdint>
#include <editor/shell/editor-terrains.h>
#include <engine/render-ground/ground-runs.h>
#include <limits>
#include <string>
#include <vector>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The encoding every tile layer is written in.
  constexpr std::string_view RLE = "rle";

  /// `tile:none`, then `tile:<word>` for every terrain in number order, so
  /// a palette number and a terrain number are the same when written.
  json paletteJson() {
    json palette = json::array();
    for (size_t terrain = 0; terrain <= EDITOR_TERRAIN_COUNT; ++terrain) {
      palette.push_back(
          std::string(EDITOR_TILE_REF_PREFIX) +
          std::string(editorTerrainWord(static_cast<uint8_t>(terrain))));
    }
    return palette;
  }

  /// Each palette number a file uses, as the terrain number it names.
  std::vector<uint8_t> readPalette(const json& content) {
    std::vector<uint8_t> terrains;
    const auto found = content.find("tile_palette");
    if (found == content.end() || !found->is_array()) {
      return terrains;
    }
    for (const json& entry : *found) {
      const std::optional<uint8_t> terrain =
          entry.is_string() ? editorTerrainNamed(entry.get<std::string>())
                            : std::nullopt;
      terrains.push_back(terrain.value_or(uint8_t{0}));
    }
    return terrains;
  }

  /// An integer at @p key of @p entry, or nothing when it is missing or is
  /// not a whole number that fits.
  std::optional<int64_t> readInteger(const json& entry, const char* key) {
    const auto found = entry.find(key);
    if (found == entry.end() || !found->is_number_integer()) {
      return std::nullopt;
    }
    return found->get<int64_t>();
  }

  /// The rectangle a file's `bounds` gives, or nothing when it is missing
  /// or holds a number that is not a cell coordinate.
  std::optional<GroundRect> readBounds(const json& content) {
    const json bounds = content.value("bounds", json::object());
    const char* keys[] = {"min_x", "min_y", "width", "height"};
    int32_t values[4] = {};
    for (size_t i = 0; i < 4; ++i) {
      const std::optional<int64_t> value = readInteger(bounds, keys[i]);
      if (!value || *value < -GROUND_COORDINATE_LIMIT ||
          *value > 2 * GROUND_COORDINATE_LIMIT) {
        return std::nullopt;
      }
      values[i] = static_cast<int32_t>(*value);
    }
    return GroundRect{values[0], values[1], values[2], values[3]};
  }

  /// One `[palette, length]` pair as a run of terrain, or nothing when it
  /// is not one.
  std::optional<GroundRun> readRun(const json& pair,
                                   const std::vector<uint8_t>& palette) {
    if (!pair.is_array() || pair.size() != 2 || !pair[0].is_number_unsigned() ||
        !pair[1].is_number_unsigned()) {
      return std::nullopt;
    }
    const auto index = pair[0].get<uint64_t>();
    const auto length = pair[1].get<uint64_t>();
    if (length > std::numeric_limits<uint32_t>::max()) {
      return std::nullopt;
    }
    return GroundRun{index < palette.size() ? palette[index] : uint8_t{0},
                     static_cast<uint32_t>(length)};
  }

  /// The terrain layer's runs, or nothing when any of them cannot be read.
  std::optional<std::vector<GroundRun>>
  readRuns(const json& layer, const std::vector<uint8_t>& palette) {
    const auto found = layer.find("runs");
    if (layer.value("encoding", "") != RLE || found == layer.end() ||
        !found->is_array()) {
      return std::nullopt;
    }
    std::vector<GroundRun> runs;
    for (const json& pair : *found) {
      const std::optional<GroundRun> run = readRun(pair, palette);
      if (!run) {
        return std::nullopt;
      }
      runs.push_back(*run);
    }
    return runs;
  }

}  // namespace

void writeEditorGround(const GroundGrid& ground, json& content) {
  const GroundRect bounds = ground.paintedBounds();
  if (bounds.width == 0) {
    return;
  }
  json runs = json::array();
  for (const GroundRun& run : encodeGroundRuns(ground, bounds)) {
    runs.push_back(json::array({run.terrain, run.length}));
  }
  content["bounds"] = {{"min_x", bounds.x},
                       {"min_y", bounds.y},
                       {"width", bounds.width},
                       {"height", bounds.height}};
  content["tile_palette"] = paletteJson();
  content["layers"] = {
      {"terrain", {{"encoding", RLE}, {"runs", std::move(runs)}}}};
}

GroundGrid readEditorGround(const json& content) {
  const json layers = content.value("layers", json::object());
  const auto terrain =
      layers.is_object() ? layers.find("terrain") : layers.end();
  const std::optional<GroundRect> bounds = readBounds(content);
  if (terrain == layers.end() || !terrain->is_object() || !bounds) {
    return {};
  }
  const std::optional<std::vector<GroundRun>> runs =
      readRuns(*terrain, readPalette(content));
  if (!runs) {
    return {};
  }
  return decodeGroundRuns(*bounds, *runs).value_or(GroundGrid{});
}

}  // namespace eng::editor
