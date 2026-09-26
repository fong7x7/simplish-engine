#include "agent-interface.h"

#include "agent-call.h"

#include <optional>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The scales the interface may be drawn at.
  constexpr double MIN_SCALE = 0.5;
  constexpr double MAX_SCALE = 3.0;
  /// The deepest `get_widgets` may list.
  constexpr size_t MAX_DEPTH = 32;

}  // namespace

AgentResult runAgentSetInterfaceSize(EditorShellState& state,
                                     const json& params) {
  const std::optional<double> scale = agentNumberParam(params, "scale");
  if (!scale || *scale < MIN_SCALE || *scale > MAX_SCALE) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "scale must be a number from 0.5 to 3");
  }
  state.graphics.ui_scale = static_cast<float>(*scale);
  ++state.graphics.revision;
  const json answer{{"interface_scale", state.graphics.ui_scale},
                    {"file", state.graphics.file.string()}};
  return agentOk(answer.dump());
}

AgentResult runAgentGetWidgets(EditorShellState&, const json& params) {
  const std::optional<size_t> depth = agentIndexParam(params, "depth");
  if (params.contains("depth") && (!depth || *depth > MAX_DEPTH)) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "depth must be a whole number from 0 to 32");
  }
  AgentResult result = agentOk(json{{"queued", "describe widgets"}}.dump());
  result.host.kind = AgentHostRequestKind::DESCRIBE_WIDGETS;
  result.host.widgets = {
      .under = agentStringParam(params, "under").value_or(""),
      .depth = depth.value_or(EditorWidgetQuery{}.depth),
      .hidden = agentBoolParam(params, "hidden").value_or(false)};
  return result;
}

}  // namespace eng::editor
