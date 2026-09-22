#include "agent-controls.h"

#include "agent-call.h"

#include <editor/shell/editor-playtest-controls.h>
#include <engine/client/desktop-key-names.h>
#include <engine/input/input-bindings-json.h>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// @p text split at its commas, each piece trimmed, empty pieces dropped.
  std::vector<std::string> splitControls(const std::string& text) {
    std::vector<std::string> out;
    std::stringstream in(text);
    std::string piece;
    while (std::getline(in, piece, ',')) {
      const size_t first = piece.find_first_not_of(" \t");
      const size_t last = piece.find_last_not_of(" \t");
      if (first != std::string::npos) {
        out.push_back(piece.substr(first, last - first + 1));
      }
    }
    return out;
  }

  /// The controls @p text lists, or the first one that is not a control.
  struct ParsedControls {
    /// Every control, in order.
    std::vector<input::InputSource> sources;
    /// The first piece that named nothing, if any.
    std::optional<std::string> bad;
  };

  /// Parse @p text's comma-separated controls.
  ParsedControls parseControls(const std::string& text) {
    ParsedControls out;
    for (const std::string& piece : splitControls(text)) {
      const std::optional<input::InputSource> source =
          input::parseInputSource(piece, client::desktopKeyNames());
      if (!source) {
        out.bad = piece;
        return out;
      }
      out.sources.push_back(*source);
    }
    return out;
  }

  /// The controls as `get_controls` reports them.
  std::string controlsPayload(const EditorShellState& state) {
    json root = json::parse(input::writeInputBindings(
        state.controls.bindings, client::desktopKeyNames()));
    root["file"] = state.controls.file.string();
    return root.dump();
  }

  /// @p bindings with the call's deadzones applied over what they had.
  void applyDeadzones(input::InputBindings& bindings, const json& params) {
    const input::GamepadDeadzones& was = bindings.deadzones();
    bindings.setDeadzones(
        {agentFloatParam(params, "left_stick", was.left_stick),
         agentFloatParam(params, "right_stick", was.right_stick),
         agentFloatParam(params, "trigger", was.trigger)});
  }

  /// Give @p action exactly the controls @p text lists in @p bindings, or
  /// a failure naming what was wrong.
  std::optional<AgentResult> rebind(input::InputBindings& bindings,
                                    const std::string& action_name,
                                    const std::string& text) {
    const std::optional<input::InputAction> action =
        input::inputActionNamed(action_name);
    if (!action) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "no action is called " + action_name);
    }
    const ParsedControls parsed = parseControls(text);
    if (parsed.bad) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          *parsed.bad + " is not a control");
    }
    bindings.clear(*action);
    for (const input::InputSource source : parsed.sources) {
      bindings.bind(*action, source);
    }
    return std::nullopt;
  }

  /// The call's `action` rebound to its `controls` in @p bindings, when it
  /// names one, or a failure; nothing when all is well.
  std::optional<AgentResult> rebindFromCall(input::InputBindings& bindings,
                                            const json& params) {
    const std::optional<std::string> action =
        agentStringParam(params, "action");
    const std::optional<std::string> controls =
        agentStringParam(params, "controls");
    if (action.has_value() != controls.has_value()) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "action and controls go together");
    }
    return action ? rebind(bindings, *action, *controls) : std::nullopt;
  }

  /// What the call starts from: a copy of the controls, so a refusal
  /// changes nothing, or the defaults with their deadzones kept for a
  /// reset.
  input::InputBindings startingBindings(const EditorShellState& state,
                                        const json& params) {
    input::InputBindings next = agentBoolParam(params, "reset").value_or(false)
                                    ? editorDefaultInputBindings()
                                    : state.controls.bindings;
    next.setDeadzones(state.controls.bindings.deadzones());
    return next;
  }

}  // namespace

AgentResult runAgentGetControls(EditorShellState& state,
                                const json& /*params*/) {
  return agentOk(controlsPayload(state));
}

AgentResult runAgentSetControls(EditorShellState& state, const json& params) {
  input::InputBindings next = startingBindings(state, params);
  if (std::optional<AgentResult> refused = rebindFromCall(next, params)) {
    return *refused;
  }
  applyDeadzones(next, params);
  state.controls.bindings = std::move(next);
  ++state.controls.revision;
  return agentOk(controlsPayload(state));
}

}  // namespace eng::editor
