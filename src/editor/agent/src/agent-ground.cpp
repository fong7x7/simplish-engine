#include "agent-ground.h"

#include "agent-call.h"

#include <cmath>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-ground-ops.h>
#include <editor/shell/editor-terrains.h>
#include <optional>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// What `paint_ground` says when it is called wrongly.
  constexpr std::string_view PAINT_GROUND_USAGE =
      "terrain, x and y are required; terrain is one of the words get_ground "
      "lists, or \"none\" to erase; width and height, when given, are whole "
      "numbers from 1 to 256";

  /// What `get_ground` says when the window is wrong.
  constexpr std::string_view GET_GROUND_USAGE =
      "x, y, width and height name a window of cells, all four or none, of "
      "at most 65536 cells; ask for a smaller window";

  /// One character per terrain number in a row string: `.` for bare
  /// ground, then the terrain's number.
  char cellChar(uint8_t terrain) {
    return terrain == 0 ? '.' : static_cast<char>('0' + terrain);
  }

  /// Every terrain, bare ground first, by number, word and name.
  json terrainsJson() {
    json terrains = json::array();
    for (size_t number = 0; number <= EDITOR_TERRAIN_COUNT; ++number) {
      const auto terrain = static_cast<uint8_t>(number);
      const EditorTerrain* found = editorTerrainAt(terrain);
      terrains.push_back({{"number", number},
                          {"terrain", editorTerrainWord(terrain)},
                          {"name", found != nullptr ? found->name : "Bare"},
                          {"char", std::string(1, cellChar(terrain))}});
    }
    return terrains;
  }

  /// A rectangle as this API reports one.
  json rectJson(const GroundRect& rect) {
    return {{"x", rect.x},
            {"y", rect.y},
            {"width", rect.width},
            {"height", rect.height}};
  }

  /// The rows of @p window, southmost first, each west to east.
  json rowsJson(const GroundGrid& ground, const GroundRect& window) {
    json rows = json::array();
    for (int32_t row = 0; row < window.height; ++row) {
      std::string line;
      for (int32_t column = 0; column < window.width; ++column) {
        line += cellChar(ground.at({window.x + column, window.y + row}));
      }
      rows.push_back(std::move(line));
    }
    return rows;
  }

  /// A whole-tile coordinate from @p key, held inside the grid's limit.
  std::optional<int32_t> tileParam(const json& params, std::string_view key) {
    const std::optional<double> value = agentNumberParam(params, key);
    if (!value || !std::isfinite(*value) ||
        std::abs(*value) > GROUND_COORDINATE_LIMIT) {
      return std::nullopt;
    }
    return static_cast<int32_t>(std::floor(*value));
  }

  /// A side of a rectangle from @p key: @p fallback when absent, nothing
  /// when it is not a whole number from 1 to @p most.
  std::optional<int32_t> sideParam(const json& params, std::string_view key,
                                   int32_t fallback, int64_t most) {
    if (!params.contains(key)) {
      return fallback;
    }
    const std::optional<size_t> value = agentIndexParam(params, key);
    if (!value || *value == 0 || static_cast<int64_t>(*value) > most) {
      return std::nullopt;
    }
    return static_cast<int32_t>(*value);
  }

  /// The window a `get_ground` call names, or the painted part of the
  /// ground when it names none. Nothing when it names one wrongly.
  std::optional<GroundRect> readWindow(const EditorShellState& state,
                                       const json& params) {
    if (!params.contains("x") && !params.contains("y")) {
      return state.document.ground.paintedBounds();
    }
    const std::optional<int32_t> x = tileParam(params, "x");
    const std::optional<int32_t> y = tileParam(params, "y");
    const std::optional<int32_t> width =
        sideParam(params, "width", 0, AGENT_GROUND_READ_MAX_CELLS);
    const std::optional<int32_t> height =
        sideParam(params, "height", 0, AGENT_GROUND_READ_MAX_CELLS);
    if (!x || !y || !width || !height || *width == 0 || *height == 0) {
      return std::nullopt;
    }
    return GroundRect{*x, *y, *width, *height};
  }

  /// The rectangle a `paint_ground` call fills, or nothing when it names
  /// one wrongly.
  std::optional<GroundRect> readFill(const json& params) {
    const std::optional<int32_t> x = tileParam(params, "x");
    const std::optional<int32_t> y = tileParam(params, "y");
    const std::optional<int32_t> width =
        sideParam(params, "width", 1, EDITOR_GROUND_FILL_MAX);
    const std::optional<int32_t> height =
        sideParam(params, "height", 1, EDITOR_GROUND_FILL_MAX);
    if (!x || !y || !width || !height) {
      return std::nullopt;
    }
    return GroundRect{*x, *y, *width, *height};
  }

  /// Make @p painted the document's ground as one undoable edit, and report
  /// how many cells that changed. Painting what is already there is no
  /// edit, and leaves no undo entry.
  AgentResult recordPaint(EditorShellState& state, const GroundGrid& painted) {
    std::vector<EditorGroundChange> changes =
        diffEditorGround(state.document.ground, painted);
    const json out{{"changed", changes.size()},
                   {"painted", rectJson(painted.paintedBounds())}};
    if (changes.empty()) {
      return agentOk(out.dump(2));
    }
    performEditorAction(
        state.history, state.document,
        {.kind = EditorActionKind::PAINT_GROUND, .ground = std::move(changes)});
    return agentEdited(out.dump(2));
  }

}  // namespace

AgentResult runAgentGetGround(const EditorShellState& state,
                              const json& params) {
  const std::optional<GroundRect> window = readWindow(state, params);
  if (!window || static_cast<int64_t>(window->width) * window->height >
                     AGENT_GROUND_READ_MAX_CELLS) {
    return agentFailure(AgentStatus::BAD_PARAMS, GET_GROUND_USAGE);
  }
  const json out{{"terrains", terrainsJson()},
                 {"painted", rectJson(state.document.ground.paintedBounds())},
                 {"window", rectJson(*window)},
                 {"rows", rowsJson(state.document.ground, *window)}};
  return agentOk(out.dump(2));
}

AgentResult runAgentPaintGround(EditorShellState& state, const json& params) {
  const std::optional<uint8_t> terrain = editorTerrainNamed(
      agentStringParam(params, "terrain").value_or(std::string{}));
  const std::optional<GroundRect> fill = readFill(params);
  if (!terrain || !fill) {
    return agentFailure(AgentStatus::BAD_PARAMS, PAINT_GROUND_USAGE);
  }
  GroundGrid painted = state.document.ground;
  paintEditorGround(painted, *fill, *terrain);
  return recordPaint(state, painted);
}

}  // namespace eng::editor
