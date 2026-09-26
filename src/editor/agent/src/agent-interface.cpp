#include "agent-interface.h"

#include "agent-call.h"

#include <optional>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The scales the interface may be drawn at.
  constexpr double MIN_SCALE = 0.5;
  constexpr double MAX_SCALE = 3.0;

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

}  // namespace eng::editor
