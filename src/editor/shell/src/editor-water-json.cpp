#include "editor-ground-json.h"

#include <array>
#include <editor/shell/editor-terrains.h>
#include <engine/render-ground/ground-runs.h>
#include <engine/render-water/water-depth.h>
#include <limits>
#include <string>
#include <vector>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The encoding every layer is written in.
  constexpr std::string_view RLE = "rle";

  /// The palette reference water was painted with before it was a layer
  /// of its own.
  constexpr std::string_view LEGACY_WATER_REF = "tile:water";

  /// What cells of that water are given under them: the sand the old
  /// stacking drew there.
  constexpr std::string_view LEGACY_BED = "sand";

  /// How many grids a water layer has.
  constexpr size_t CHANNEL_COUNT = 7;

  /// A water layer's grids by the keys a file writes them under, in the
  /// order `WaterLayer` holds them.
  constexpr std::array<const char*, CHANNEL_COUNT> CHANNEL_KEYS{
      "depth", "red", "green", "blue", "opacity", "flow_heading", "flow_speed"};

  /// How many of them every file has: the flow came later, and a file
  /// without it holds standing water.
  constexpr size_t REQUIRED_CHANNELS = 5;

  /// @p layer's grids in `CHANNEL_KEYS`' order.
  std::array<const GroundGrid*, CHANNEL_COUNT>
  channelsOf(const WaterLayer& layer) {
    return {&layer.depth,   &layer.red,          &layer.green,     &layer.blue,
            &layer.opacity, &layer.flow_heading, &layer.flow_speed};
  }

  /// The same, to write into.
  std::array<GroundGrid*, CHANNEL_COUNT> channelsOf(WaterLayer& layer) {
    return {&layer.depth,   &layer.red,          &layer.green,     &layer.blue,
            &layer.opacity, &layer.flow_heading, &layer.flow_speed};
  }

  /// @p grid over @p bounds as `[value, run_length]` pairs.
  json runsJson(const GroundGrid& grid, GroundRect bounds) {
    json runs = json::array();
    for (const GroundRun& run : encodeGroundRuns(grid, bounds)) {
      runs.push_back(json::array({run.terrain, run.length}));
    }
    return runs;
  }

  /// A whole number at @p key of @p object, or nothing.
  std::optional<int32_t> readInt(const json& object, const char* key) {
    const auto found = object.find(key);
    if (found == object.end() || !found->is_number_integer()) {
      return std::nullopt;
    }
    const auto value = found->get<int64_t>();
    return value < -2 * GROUND_COORDINATE_LIMIT ||
                   value > 2 * GROUND_COORDINATE_LIMIT
               ? std::nullopt
               : std::optional<int32_t>(static_cast<int32_t>(value));
  }

  /// The rectangle an object's `bounds` names, or nothing.
  std::optional<GroundRect> readRect(const json& object) {
    const json bounds = object.value("bounds", json::object());
    const auto x = readInt(bounds, "min_x");
    const auto y = readInt(bounds, "min_y");
    const auto width = readInt(bounds, "width");
    const auto height = readInt(bounds, "height");
    if (!x || !y || !width || !height) {
      return std::nullopt;
    }
    return GroundRect{*x, *y, *width, *height};
  }

  /// `[value, run_length]` pairs, each value mapped through @p map, or
  /// nothing when any pair is not one.
  std::optional<std::vector<GroundRun>>
  readPairs(const json& runs, const std::array<uint8_t, 256>& map) {
    if (!runs.is_array()) {
      return std::nullopt;
    }
    std::vector<GroundRun> out;
    for (const json& pair : runs) {
      if (!pair.is_array() || pair.size() != 2 ||
          !pair[0].is_number_unsigned() || !pair[1].is_number_unsigned() ||
          pair[0].get<uint64_t>() > 255 ||
          pair[1].get<uint64_t>() > std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
      }
      out.push_back({map[pair[0].get<size_t>()],
                     static_cast<uint32_t>(pair[1].get<uint64_t>())});
    }
    return out;
  }

  /// Every byte as itself.
  std::array<uint8_t, 256> identityMap() {
    std::array<uint8_t, 256> map{};
    for (size_t i = 0; i < map.size(); ++i) {
      map[i] = static_cast<uint8_t>(i);
    }
    return map;
  }

  /// One grid of a water layer from the runs under @p key, or nothing —
  /// or, for one a file may leave out, zero over @p bounds when it does.
  std::optional<GroundGrid> readChannel(const json& layer, size_t channel,
                                        GroundRect bounds) {
    const char* key = CHANNEL_KEYS[channel];
    if (channel >= REQUIRED_CHANNELS && !layer.contains(key)) {
      const std::vector<GroundRun> still{
          {0, static_cast<uint32_t>(bounds.width * bounds.height)}};
      return decodeGroundRuns(bounds, still);
    }
    const auto runs = readPairs(layer.value(key, json()), identityMap());
    return runs ? decodeGroundRuns(bounds, *runs) : std::nullopt;
  }

  /// The water layer a file's `layers.water` holds, or nothing when any of
  /// it cannot be read.
  std::optional<WaterLayer> readLayer(const json& layer) {
    const std::optional<GroundRect> bounds = readRect(layer);
    if (!bounds || layer.value("encoding", "") != RLE ||
        layer.value("step", 0.0) != static_cast<double>(WATER_DEPTH_STEP)) {
      return std::nullopt;
    }
    WaterLayer water;
    const std::array<GroundGrid*, CHANNEL_COUNT> channels = channelsOf(water);
    for (size_t i = 0; i < channels.size(); ++i) {
      std::optional<GroundGrid> grid = readChannel(layer, i, *bounds);
      if (!grid) {
        return std::nullopt;
      }
      *channels[i] = std::move(*grid);
    }
    return water;
  }

  /// A map from a file's tile palette to 1 for its water and 0 for every
  /// other terrain.
  std::array<uint8_t, 256> legacyWaterMap(const json& content) {
    std::array<uint8_t, 256> map{};
    const json palette = content.value("tile_palette", json::array());
    for (size_t i = 0; i < palette.size() && i < map.size(); ++i) {
      map[i] = palette[i] == LEGACY_WATER_REF ? 1 : 0;
    }
    return map;
  }

  /// Which cells a level written before water was a layer painted water,
  /// as 1s over its bounds; empty when none did.
  GroundGrid legacyWaterCells(const json& content) {
    const json layers = content.value("layers", json::object());
    const json terrain = layers.is_object()
                             ? layers.value("terrain", json::object())
                             : json::object();
    const std::optional<GroundRect> bounds = readRect(content);
    const auto runs =
        readPairs(terrain.value("runs", json()), legacyWaterMap(content));
    return bounds && runs
               ? decodeGroundRuns(*bounds, *runs).value_or(GroundGrid{})
               : GroundGrid{};
  }

  /// The depth such a level gave a cell of its water: its old
  /// `water_depth` layer's, where zero meant the default.
  uint8_t legacyDepth(const GroundGrid& depths, GroundCell cell) {
    const uint8_t units = depths.at(cell);
    return units != 0 ? units : waterDepthUnits(WATER_DEFAULT_DEPTH);
  }

  /// The old `layers.water_depth`, over the ground's bounds; empty when
  /// there is none.
  GroundGrid legacyDepths(const json& content) {
    const json layers = content.value("layers", json::object());
    const json depth = layers.is_object()
                           ? layers.value("water_depth", json::object())
                           : json::object();
    const std::optional<GroundRect> bounds = readRect(content);
    const auto runs = readPairs(depth.value("runs", json()), identityMap());
    return bounds && runs
               ? decodeGroundRuns(*bounds, *runs).value_or(GroundGrid{})
               : GroundGrid{};
  }

  /// Water as a level from before it was a layer painted it: each cell of
  /// it laid as water in the default colour at its old depth, and given
  /// the sand that was drawn under it on @p ground.
  WaterLayer migrateLegacyWater(const json& content, GroundGrid& ground) {
    const GroundGrid cells = legacyWaterCells(content);
    const GroundGrid depths = legacyDepths(content);
    const uint8_t sand = editorTerrainNamed(LEGACY_BED).value_or(0);
    WaterLayer water;
    const GroundRect bounds = cells.bounds();
    for (int32_t i = 0; i < bounds.width * bounds.height; ++i) {
      const GroundCell cell{bounds.x + i % bounds.width,
                            bounds.y + i / bounds.width};
      if (cells.at(cell) != 0) {
        setWaterCell(water, cell, {.depth = legacyDepth(depths, cell)});
        ground.set(cell, sand);
      }
    }
    return water;
  }

}  // namespace

void writeEditorWater(const WaterLayer& water, json& content) {
  const GroundRect bounds = waterLayerBounds(water);
  if (bounds.width == 0) {
    return;
  }
  json layer{{"bounds",
              {{"min_x", bounds.x},
               {"min_y", bounds.y},
               {"width", bounds.width},
               {"height", bounds.height}}},
             {"encoding", RLE},
             {"step", WATER_DEPTH_STEP}};
  const std::array<const GroundGrid*, CHANNEL_COUNT> channels =
      channelsOf(water);
  for (size_t i = 0; i < channels.size(); ++i) {
    layer[CHANNEL_KEYS[i]] = runsJson(*channels[i], bounds);
  }
  content["layers"]["water"] = std::move(layer);
}

WaterLayer readEditorWater(const json& content, GroundGrid& ground) {
  const json layers = content.value("layers", json::object());
  if (layers.is_object() && layers.contains("water")) {
    return readLayer(layers.at("water")).value_or(WaterLayer{});
  }
  return migrateLegacyWater(content, ground);
}

}  // namespace eng::editor
