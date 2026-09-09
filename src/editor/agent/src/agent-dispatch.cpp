#include "agent-call.h"
#include "agent-commands.h"

#include <cstddef>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-state-json.h>
#include <editor/agent/agent-tool-info.h>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// Every tool has this shape, whether or not it reads its parameters or
  /// edits the state it is given. One signature is what lets the table
  /// below be a table rather than twenty-one branches.
  using AgentToolFn = AgentResult (*)(EditorShellState&, const json&);

  AgentResult toolDescribe(EditorShellState& state, const json&) {
    return agentOk(agentDescribeJson(state));
  }

  AgentResult toolGetState(EditorShellState& state, const json&) {
    return agentOk(agentStateJson(state));
  }

  AgentResult toolListAssets(EditorShellState& state, const json&) {
    return agentOk(agentAssetsJson(state));
  }

  AgentResult toolListFolders(EditorShellState& state, const json&) {
    return agentOk(agentFoldersJson(state));
  }

  AgentResult toolListPlacements(EditorShellState& state, const json&) {
    return agentOk(agentPlacementsJson(state));
  }

  AgentResult toolListLights(EditorShellState& state, const json&) {
    return agentOk(agentLightsJson(state));
  }

  AgentResult toolGetSelection(EditorShellState& state, const json&) {
    return agentOk(agentSelectionJson(state));
  }

  AgentResult toolGetHistory(EditorShellState& state, const json&) {
    return agentOk(agentHistoryJson(state));
  }

  AgentResult toolListCommands(EditorShellState& state, const json&) {
    return agentOk(agentCommandsJson(state));
  }

  AgentResult toolUndo(EditorShellState& state, const json&) {
    return runAgentUndo(state);
  }

  AgentResult toolRedo(EditorShellState& state, const json&) {
    return runAgentRedo(state);
  }

  AgentResult toolRunCommand(EditorShellState& state, const json& params) {
    return runAgentRunCommand(state, params);
  }

  AgentResult toolOpenProject(EditorShellState&, const json& params) {
    return runAgentOpenProject(params);
  }

  AgentResult toolRescanAssets(EditorShellState& state, const json&) {
    return runAgentRescanAssets(state);
  }

  /// What answers each tool, in `AgentTool` order.
  ///
  /// A table rather than a switch, for the reason `EDITOR_PROPERTY_TRAITS`
  /// is one: twenty-one two-line arms say no more than twenty-one rows, and
  /// the assertion below catches the tool added to the enum without an
  /// answer here.
  constexpr AgentToolFn AGENT_TOOL_FNS[] = {
      toolDescribe,
      toolGetState,
      toolListAssets,
      runAgentGetAsset,
      toolListFolders,
      toolListPlacements,
      toolListLights,
      toolGetSelection,
      toolGetHistory,
      toolListCommands,
      runAgentPlaceAsset,
      runAgentAddLight,
      runAgentSetProperty,
      runAgentTranslate,
      runAgentSelect,
      runAgentSetTool,
      toolRunCommand,
      toolUndo,
      toolRedo,
      toolOpenProject,
      toolRescanAssets,
  };

  static_assert(std::size(AGENT_TOOL_FNS) == std::size(AGENT_TOOLS),
                "every agent tool needs something that answers it");

  /// Parse a request or parameter body. An empty body is an empty object,
  /// which is what a tool taking no parameters is called with.
  std::optional<json> parseAgentJson(std::string_view text) {
    if (text.empty()) {
      return json::object();
    }
    json parsed = json::parse(std::string(text), nullptr, false);
    if (parsed.is_discarded()) {
      return std::nullopt;
    }
    return parsed;
  }

  /// Run the tool called @p tool with parameters already parsed.
  AgentResult runNamedTool(EditorShellState& state, std::string_view tool,
                           const json& params) {
    const std::optional<AgentTool> found = findAgentTool(tool);
    if (!found) {
      return agentFailure(AgentStatus::UNKNOWN_TOOL,
                          "no tool is called that; describe lists every one");
    }
    return AGENT_TOOL_FNS[static_cast<size_t>(*found)](state, params);
  }

}  // namespace

AgentResult runAgentTool(EditorShellState& state, std::string_view tool,
                         std::string_view params_json) {
  const std::optional<json> params = parseAgentJson(params_json);
  if (!params || !params->is_object()) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "params must be a JSON object");
  }
  return runNamedTool(state, tool, *params);
}

AgentResult runAgentRequest(EditorShellState& state,
                            std::string_view request_json) {
  const std::optional<json> request = parseAgentJson(request_json);
  if (!request || !request->is_object()) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "expected a JSON object of the form "
                        "{\"tool\": \"...\", \"params\": {...}}");
  }
  const std::optional<std::string> tool = agentStringParam(*request, "tool");
  if (!tool) {
    return agentFailure(AgentStatus::BAD_PARAMS, "tool is required");
  }
  const json params = request->value("params", json::object());
  return runNamedTool(state, *tool, params);
}

std::string agentResponseJson(const AgentResult& result) {
  json payload = json::parse(result.json, nullptr, false);
  return json{{"status", agentStatusName(result.status)},
              {"changed", result.changed},
              {"result", payload.is_discarded() ? json(nullptr) : payload}}
      .dump(2);
}

}  // namespace eng::editor
