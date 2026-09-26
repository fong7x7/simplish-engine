#include "agent-host-outcome.h"

#include "agent-build.h"
#include "agent-ui.h"

#include <editor/agent/agent-state-json.h>
#include <editor/shell/editor-menu-availability.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The part of @p state the menu command @p command changes: the build
  /// for a Build row, the playtest for Play, the rest of the state for the
  /// others.
  std::string commandPart(const EditorShellState& state,
                          EditorMenuCommand command) {
    if (editorMenuCommandBuilds(command)) {
      return agentBuildJson(state);
    }
    return command == EditorMenuCommand::PLAYTEST ? agentPlaytestJson(state)
                                                  : agentStateJson(state);
  }

  /// The part of @p state a request of @p kind about the project changes:
  /// its screens, a screen drawn, or the rest of the state.
  std::string projectPart(const EditorShellState& state,
                          AgentHostRequestKind kind) {
    if (kind == AgentHostRequestKind::WRITE_UI_SCREEN ||
        kind == AgentHostRequestKind::WRITE_UI_THEME) {
      return agentUiScreensJson(state);
    }
    return kind == AgentHostRequestKind::RENDER_UI_SCREEN
               ? agentUiRenderJson(state)
               : agentStateJson(state);
  }

  /// The part of @p state @p request changes, as its read tool reports it;
  /// empty for a request that changes nothing a caller reads back.
  std::string changedPart(const EditorShellState& state,
                          const AgentHostRequest& request) {
    switch (request.kind) {
      case AgentHostRequestKind::NONE:
      case AgentHostRequestKind::PLAY_EFFECT:
      case AgentHostRequestKind::PLAY_SOUND:
        return {};
      case AgentHostRequestKind::START_PLAYTEST:
      case AgentHostRequestKind::STEP_PLAYTEST:
        return agentPlaytestJson(state);
      case AgentHostRequestKind::RUN_COMMAND:
        return commandPart(state, request.command);
      default:
        return projectPart(state, request.kind);
    }
  }

}  // namespace

std::string agentHostOutcomeJson(const EditorShellState& state,
                                 const AgentResult& result) {
  const std::string part = changedPart(state, result.host);
  const json queued = json::parse(result.json, nullptr, false);
  json outcome = json::parse(part, nullptr, false);
  if (part.empty() || !outcome.is_object() || !queued.is_object()) {
    return result.json;
  }
  outcome["ran"] = queued.contains("queued") ? queued["queued"] : json();
  return outcome.dump(2);
}

}  // namespace eng::editor
