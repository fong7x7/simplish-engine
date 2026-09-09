#pragma once

/// @file agent-dispatch.h
/// @brief Run one agent tool against the editor's state.
/// @par Threading Main-thread-only (edits shell state).

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <string>
#include <string_view>

namespace eng::editor {

/// Run the tool called @p tool with @p params_json against @p state.
///
/// A pure function of the state and the call, but for the two things it
/// cannot do itself, which it hands back in `AgentResult::host`. That is
/// what lets the whole tool surface be tested against a bare
/// `EditorShellState` with no window, no GPU, and no socket.
///
/// @p params_json is a JSON object, or empty for a tool that takes none.
/// Anything else is `BAD_PARAMS` rather than a crash: the parameters come
/// off a socket and are not to be trusted.
[[nodiscard]] AgentResult runAgentTool(EditorShellState& state,
                                       std::string_view tool,
                                       std::string_view params_json);

/// Run one request of the form `{"tool": "...", "params": {...}}`.
///
/// The shape the HTTP endpoint and the MCP bridge both post. A body that
/// is not an object, or names no tool, is `BAD_PARAMS`.
[[nodiscard]] AgentResult runAgentRequest(EditorShellState& state,
                                          std::string_view request_json);

/// @p result as the JSON body sent back: the status word, the payload, and
/// whether the call changed anything.
[[nodiscard]] std::string agentResponseJson(const AgentResult& result);

}  // namespace eng::editor
