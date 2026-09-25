#include "agent-water.h"

#include "agent-call.h"
#include "agent-ground.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <editor/agent/agent-state-json.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-shell-selection.h>
#include <editor/shell/editor-water-depths.h>
#include <editor/shell/editor-water-ops.h>
#include <engine/render-water/water-depth.h>
#include <engine/render-water/water-fidelity.h>
#include <optional>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// Every fidelity's word, lowest first.
  json fidelityWords() {
    json words = json::array();
    for (const WaterFidelity fidelity : WATER_FIDELITIES) {
      words.push_back(waterFidelityWord(fidelity));
    }
    return words;
  }

  /// Every named depth: its word, its name and its tiles.
  json depthsJson() {
    json depths = json::array();
    for (const EditorWaterDepth& depth : EDITOR_WATER_DEPTHS) {
      depths.push_back({{"depth", depth.word},
                        {"name", depth.name},
                        {"tiles", depth.tiles}});
    }
    return depths;
  }

  /// Tiles of water: the wet samples, over the samples a tile holds.
  double waterTiles(const EditorWaterState& water) {
    const double per_tile =
        static_cast<double>(water.samples_per_tile) * water.samples_per_tile;
    return per_tile > 0.0 ? water.wet_samples / per_tile : 0.0;
  }

  /// Which of the water's effects are drawn, by word.
  json effectsJson(const WaterEffects& effects) {
    json out = json::object();
    for (const WaterEffect effect : WATER_EFFECT_LIST) {
      out[std::string(waterEffectWord(effect))] =
          waterEffectOn(effects, effect);
    }
    return out;
  }

  /// @p params' switches over @p effects, or nothing when one given is not
  /// a boolean.
  std::optional<WaterEffects> effectsParam(const json& params,
                                           WaterEffects effects) {
    for (const WaterEffect effect : WATER_EFFECT_LIST) {
      const std::string word(waterEffectWord(effect));
      if (!params.contains(word)) {
        continue;
      }
      const std::optional<bool> on = agentBoolParam(params, word);
      if (!on) {
        return std::nullopt;
      }
      effects.on[waterEffectIndex(effect)] = *on;
    }
    return effects;
  }

  /// Everything `get_water` reports.
  std::string waterPayload(const EditorShellState& state) {
    const EditorWaterState& water = state.water;
    const json root{{"fidelity", waterFidelityWord(state.graphics.water)},
                    {"fidelities", fidelityWords()},
                    {"depths", depthsJson()},
                    {"file", state.graphics.file.string()},
                    {"drawn", water.drawn},
                    {"samples_per_tile", water.samples_per_tile},
                    {"wet_samples", water.wet_samples},
                    {"water_tiles", waterTiles(water)},
                    {"energy", water.energy},
                    {"pushes", water.pushes},
                    {"splashes", water.splashes},
                    {"obstacles", water.obstacles},
                    {"effects", effectsJson(state.graphics.water_effects)}};
    return root.dump();
  }

  /// What `set_water_depth` says when it is called wrongly.
  constexpr std::string_view SET_WATER_DEPTH_USAGE =
      "depth is required: puddle, shallows, pond, lake or deep, or a number "
      "of tiles from 0.0625 to 15.9; then x and y, with width and height "
      "from 1 to 256, or target \"selection\"";

  /// What `paint_water` says when it is called wrongly.
  constexpr std::string_view PAINT_WATER_USAGE =
      "x and y are required, with width and height from 1 to 256; depth, "
      "when given, is puddle, shallows, pond, lake or deep or a number of "
      "tiles; color is \"#rrggbb\"; opacity and flow_speed are numbers "
      "from 0 to 1; flow_direction is degrees anticlockwise from east";

  /// The depth a call names at @p key, in tiles: a named depth's word or a
  /// number, as a string or as a number.
  std::optional<float> depthParam(const json& params) {
    if (const std::optional<std::string> word =
            agentStringParam(params, "depth")) {
      return editorWaterDepthNamed(*word);
    }
    const std::optional<double> tiles = agentNumberParam(params, "depth");
    return tiles ? editorWaterDepthNamed(std::to_string(*tiles)) : std::nullopt;
  }

  /// A failure for a call on the selected body of water when none is.
  AgentResult noWaterSelected() {
    return agentFailure(AgentStatus::UNAVAILABLE,
                        "no body of water is selected; select it with "
                        "target \"water\" first");
  }

  /// Whether a body of water is selected in @p state.
  bool waterSelected(const EditorShellState& state) {
    return state.selection.kind == EditorSelectionKind::WATER &&
           editorSelectableCount(state, EditorSelectionKind::WATER) > 0;
  }

  /// Make @p after the document's water as one undoable edit, and report
  /// how many cells that changed beside @p out.
  AgentResult recordWater(EditorShellState& state, const WaterLayer& after,
                          json out) {
    const std::optional<EditorAction> action =
        editorWaterEdit(state.document, after);
    out["changed"] = action ? action->water.size() : 0;
    if (!action) {
      return agentOk(out.dump(2));
    }
    performEditorAction(state.history, state.document, *action);
    return agentEdited(out.dump(2));
  }

  /// Two hex digits of @p text from @p at as a byte, or nothing.
  std::optional<uint8_t> hexByte(const std::string& text, size_t at) {
    unsigned int value = 0;
    const char* first = text.data() + at;
    const auto [end, error] = std::from_chars(first, first + 2, value, 16);
    if (error != std::errc{} || end != first + 2) {
      return std::nullopt;
    }
    return static_cast<uint8_t>(value);
  }

  /// A `#rrggbb` colour into @p water's colour, or false when @p text is
  /// not one.
  bool readHexColor(const std::string& text, WaterCell& water) {
    if (text.size() != 7 || text[0] != '#') {
      return false;
    }
    const std::optional<uint8_t> red = hexByte(text, 1);
    const std::optional<uint8_t> green = hexByte(text, 3);
    const std::optional<uint8_t> blue = hexByte(text, 5);
    if (!red || !green || !blue) {
      return false;
    }
    water.red = *red;
    water.green = *green;
    water.blue = *blue;
    return true;
  }

  /// The call's `opacity`, 0 to 1, into @p water when it gives one; false
  /// when it gives one out of range.
  bool readOpacity(const json& params, WaterCell& water) {
    if (!params.contains("opacity")) {
      return true;
    }
    const std::optional<double> opacity = agentNumberParam(params, "opacity");
    if (!opacity || *opacity < 0.0 || *opacity > 1.0) {
      return false;
    }
    water.opacity = static_cast<uint8_t>(std::lround(*opacity * 255.0));
    return true;
  }

  /// The call's `flow_direction`, in degrees, and `flow_speed`, 0 to 1,
  /// into @p water when it gives them; false when either is wrong.
  bool readFlow(const json& params, WaterCell& water) {
    const std::optional<double> heading =
        agentNumberParam(params, "flow_direction");
    const std::optional<double> speed = agentNumberParam(params, "flow_speed");
    if ((params.contains("flow_direction") && !heading) ||
        (params.contains("flow_speed") &&
         (!speed || *speed < 0.0 || *speed > 1.0))) {
      return false;
    }
    water.flow_heading =
        editorWaterByte(EditorPropertyField::FLOW_DIRECTION,
                        static_cast<float>(heading.value_or(0.0)));
    water.flow_speed = editorWaterByte(EditorPropertyField::FLOW_SPEED,
                                       static_cast<float>(speed.value_or(0.0)));
    return true;
  }

  /// The water a `paint_water` call lays: its depth, and its colour and
  /// opacity for cells that were dry; nothing when any is wrong.
  std::optional<WaterCell> waterParam(const json& params) {
    WaterCell water{};
    const std::optional<float> tiles =
        params.contains("depth") ? depthParam(params)
                                 : std::optional<float>(WATER_DEFAULT_DEPTH);
    const std::optional<std::string> color = agentStringParam(params, "color");
    if (!tiles || (params.contains("color") &&
                   (!color || !readHexColor(*color, water)))) {
      return std::nullopt;
    }
    water.depth = waterDepthUnits(*tiles);
    return readOpacity(params, water) && readFlow(params, water)
               ? std::optional<WaterCell>(water)
               : std::nullopt;
  }

}  // namespace

AgentResult runAgentSetWaterDepth(EditorShellState& state, const json& params) {
  const std::optional<float> tiles = depthParam(params);
  if (!tiles) {
    return agentFailure(AgentStatus::BAD_PARAMS, SET_WATER_DEPTH_USAGE);
  }
  const uint8_t units = waterDepthUnits(*tiles);
  const json out{{"depth", waterDepthTiles(units)},
                 {"label", editorWaterDepthLabel(units)}};
  WaterLayer deepened = state.document.water;
  if (agentStringParam(params, "target").value_or("") == "selection") {
    if (!waterSelected(state)) {
      return noWaterSelected();
    }
    setEditorWaterDepth(deepened, state.ground_selection, units);
    return recordWater(state, deepened, out);
  }
  const std::optional<GroundRect> fill = agentFillParam(params);
  if (!fill) {
    return agentFailure(AgentStatus::BAD_PARAMS, SET_WATER_DEPTH_USAGE);
  }
  setEditorWaterDepthIn(deepened, *fill, units);
  return recordWater(state, deepened, out);
}

AgentResult runAgentPaintWater(EditorShellState& state, const json& params) {
  const std::optional<GroundRect> fill = agentFillParam(params);
  const std::optional<WaterCell> water = waterParam(params);
  if (!fill || !water) {
    return agentFailure(AgentStatus::BAD_PARAMS, PAINT_WATER_USAGE);
  }
  WaterLayer painted = state.document.water;
  const bool dry = agentBoolParam(params, "dry").value_or(false);
  if (dry) {
    dryEditorWater(painted, *fill);
  } else {
    layEditorWater(painted, *fill, *water);
  }
  return recordWater(state, painted, {{"dry", dry}});
}

AgentResult agentSelectWaterAt(EditorShellState& state, const json& params) {
  const std::optional<double> x = agentNumberParam(params, "x");
  const std::optional<double> y = agentNumberParam(params, "y");
  if (!x || !y || std::abs(*x) > GROUND_COORDINATE_LIMIT ||
      std::abs(*y) > GROUND_COORDINATE_LIMIT) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "target \"water\" takes the x and y of a tile");
  }
  if (!selectEditorWater(state, {static_cast<int32_t>(std::floor(*x)),
                                 static_cast<int32_t>(std::floor(*y))})) {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "there is no water on that tile; get_ground's "
                        "water rows show where there is");
  }
  return agentEdited(agentSelectionJson(state));
}

AgentResult agentDrySelectedWater(EditorShellState& state) {
  WaterLayer dried = state.document.water;
  for (const GroundCell cell : state.ground_selection) {
    setWaterCell(dried, cell, WaterCell{.depth = 0});
  }
  state.selection = EditorSelection{};
  if (const std::optional<EditorAction> action =
          editorWaterEdit(state.document, dried)) {
    performEditorAction(state.history, state.document, *action);
  }
  return agentEdited(agentSelectionJson(state));
}

AgentResult agentSetWaterField(EditorShellState& state,
                               EditorPropertyField field, float value) {
  if (std::ranges::find(EDITOR_WATER_FIELDS, field) ==
      std::end(EDITOR_WATER_FIELDS)) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "a body of water has color_r, color_g, color_b, "
                        "opacity and flow_speed, each from 0 to 1, and "
                        "flow_direction, in degrees anticlockwise from east");
  }
  WaterLayer painted = state.document.water;
  setEditorWaterValue(painted, state.ground_selection, field, value);
  if (const std::optional<EditorAction> action =
          editorWaterEdit(state.document, painted)) {
    performEditorAction(state.history, state.document, *action);
    return agentEdited(agentSelectionJson(state));
  }
  return agentOk(agentSelectionJson(state));
}

AgentResult runAgentGetWater(EditorShellState& state, const json& /*params*/) {
  return agentOk(waterPayload(state));
}

AgentResult runAgentSetWaterFidelity(EditorShellState& state,
                                     const json& params) {
  const std::optional<std::string> word = agentStringParam(params, "fidelity");
  const std::optional<WaterFidelity> fidelity =
      word ? waterFidelityNamed(*word) : std::nullopt;
  if (!fidelity) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "fidelity must be flat, low or high");
  }
  state.graphics.water = *fidelity;
  ++state.graphics.revision;
  return agentOk(waterPayload(state));
}

AgentResult runAgentSetWaterEffects(EditorShellState& state,
                                    const json& params) {
  const std::optional<WaterEffects> effects =
      effectsParam(params, state.graphics.water_effects);
  if (!effects) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "reflections, refraction, contact and caustics are "
                        "each true or false");
  }
  state.graphics.water_effects = *effects;
  ++state.graphics.revision;
  return agentOk(waterPayload(state));
}

}  // namespace eng::editor
