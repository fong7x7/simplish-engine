/// @file dockspace-config-loader.cpp
/// @brief JSON loader implementation for `GuiDockLayout`. See the header
/// for the expected JSON schema and failure modes.

#include "engine/gui/dockspace-config-loader.h"

#include "engine/core/logger.h"
#include "engine/gui/dock-edge.h"
#include "engine/gui/dock-region.h"
#include "engine/gui/gui-widget-id.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace eng {

namespace {

  /// Subsystem tag for logger calls from this loader.
  constexpr const char* LOG_SUBSYSTEM = "DockspaceConfigLoader";

  /// Map an uppercase edge name string from JSON to the matching
  /// `DockEdge` value. Returns `std::nullopt` for unknown names.
  std::optional<DockEdge> parseEdgeName(std::string_view name) {
    if (name == "TOP") {
      return DockEdge::TOP;
    }
    if (name == "BOTTOM") {
      return DockEdge::BOTTOM;
    }
    if (name == "LEFT") {
      return DockEdge::LEFT;
    }
    if (name == "RIGHT") {
      return DockEdge::RIGHT;
    }
    if (name == "CENTRE") {
      return DockEdge::CENTRE;
    }
    return std::nullopt;
  }

  /// True if the JSON entry has the required "edge" and "size_px" keys
  /// with matching types (string and number respectively).
  bool regionEntryShapeValid(const nlohmann::json& entry) {
    if (!entry.contains("edge") || !entry.contains("size_px")) {
      return false;
    }
    return entry["edge"].is_string() && entry["size_px"].is_number();
  }

  /// Validate a numeric `size_px` value. Rejects NaN, infinity, and
  /// negative numbers.
  bool sizeValid(float size_px) {
    return std::isfinite(size_px) && size_px >= 0.0f;
  }

  /// Parse and validate the "edge" field of a region entry. Logs the
  /// offending JSON edge string on failure.
  std::optional<DockEdge> parseEdgeField(const nlohmann::json& entry) {
    if (!regionEntryShapeValid(entry)) {
      Logger::warn(LOG_SUBSYSTEM, "region missing edge/size_px fields");
      return std::nullopt;
    }
    auto edge_name = entry["edge"].get<std::string>();
    auto edge = parseEdgeName(edge_name);
    if (!edge.has_value()) {
      Logger::warn(LOG_SUBSYSTEM, "unknown edge name: " + edge_name);
      return std::nullopt;
    }
    return edge;
  }

  /// Parse and validate the "size_px" field of a region entry. Logs the
  /// offending numeric value on failure.
  std::optional<float> parseSizeField(const nlohmann::json& entry) {
    float sz = entry["size_px"].get<float>();
    if (!sizeValid(sz)) {
      Logger::warn(LOG_SUBSYSTEM, "invalid size_px (negative or non-finite): " +
                                      std::to_string(sz));
      return std::nullopt;
    }
    return sz;
  }

  /// Assemble a `DockRegion` with a placeholder child id.
  DockRegion makeRegion(DockEdge edge, float size_px) {
    DockRegion region;
    region.edge = edge;
    region.size_px = size_px;
    region.child = GUI_WIDGET_ID_INVALID;
    return region;
  }

  /// Parse a single `DockRegion` entry from JSON. Logs and returns
  /// `std::nullopt` on any validation failure.
  std::optional<DockRegion> parseRegion(const nlohmann::json& entry) {
    auto edge = parseEdgeField(entry);
    if (!edge.has_value()) {
      return std::nullopt;
    }
    auto size_px = parseSizeField(entry);
    if (!size_px.has_value()) {
      return std::nullopt;
    }
    return makeRegion(*edge, *size_px);
  }

  /// Validate that `extra_regions` (M9-reserved) is absent or empty.
  bool extraRegionsOk(const nlohmann::json& root) {
    if (!root.contains("extra_regions")) {
      return true;
    }
    if (root["extra_regions"].empty()) {
      return true;
    }
    Logger::warn(LOG_SUBSYSTEM, "extra_regions must be empty in M1");
    return false;
  }

  /// Pre-fill each slot with its correct `edge` so regions missing from
  /// JSON keep a valid (empty, well-labelled) entry rather than one
  /// carrying the default `CENTRE` edge at a non-CENTRE index.
  void preInitLayoutEdges(GuiDockLayout& layout) {
    for (size_t i = 0; i < DOCK_EDGE_COUNT; ++i) {
      layout.regions[i].edge = static_cast<DockEdge>(i);
      layout.regions[i].child = GUI_WIDGET_ID_INVALID;
      layout.regions[i].size_px = 0.0f;
    }
  }

  /// Iterate the "regions" array, parsing each entry into `layout`.
  /// Returns `false` on the first malformed entry or duplicate edge (no
  /// partial state).
  bool fillRegions(const nlohmann::json& regions_json, GuiDockLayout& layout) {
    std::array<bool, DOCK_EDGE_COUNT> seen{};
    for (const auto& entry : regions_json) {
      auto region = parseRegion(entry);
      if (!region.has_value()) {
        return false;
      }
      auto idx = static_cast<size_t>(region->edge);
      if (seen[idx]) {
        Logger::warn(LOG_SUBSYSTEM, "duplicate edge in regions array");
        return false;
      }
      seen[idx] = true;
      layout.regions[idx] = *region;
    }
    return true;
  }

  /// Parse the top-level layout object. Extracts the "regions" array and
  /// delegates region-entry parsing to `parseRegion`. Pre-initialises
  /// each slot's edge so missing-from-JSON regions remain well-labelled.
  std::optional<GuiDockLayout> parseLayout(const nlohmann::json& root) {
    if (!root.contains("regions") || !root["regions"].is_array()) {
      Logger::warn(LOG_SUBSYSTEM, "root missing 'regions' array");
      return std::nullopt;
    }
    if (!extraRegionsOk(root)) {
      return std::nullopt;
    }
    GuiDockLayout layout;
    preInitLayoutEdges(layout);
    if (!fillRegions(root["regions"], layout)) {
      return std::nullopt;
    }
    return layout;
  }

  /// Open `path` and parse it as JSON. Returns `std::nullopt` on I/O or
  /// parse failure, logging the cause with the offending path.
  std::optional<nlohmann::json> openJsonFile(std::string_view path) {
    std::string path_str(path);
    std::ifstream file(path_str);
    if (!file.is_open()) {
      Logger::warn(LOG_SUBSYSTEM, "could not open file: " + path_str);
      return std::nullopt;
    }
    auto root = nlohmann::json::parse(file, nullptr, false);
    if (root.is_discarded()) {
      Logger::warn(LOG_SUBSYSTEM, "failed to parse JSON: " + path_str);
      return std::nullopt;
    }
    return root;
  }

}  // namespace

std::optional<GuiDockLayout>
DockspaceConfigLoader::load(std::string_view path) {
  auto root = openJsonFile(path);
  if (!root.has_value()) {
    return std::nullopt;
  }
  return parseLayout(*root);
}

}  // namespace eng
