#pragma once

/// @file agent-host-outcome.h
/// @brief What a tool the editor carried out actually did, for its answer.
/// @par Threading Main-thread-only (reads shell state).

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <string>

namespace eng::editor {

/// The answer to @p result's call, once the editor has carried out its
/// host request against @p state: the part of the state it changed —
/// the playtest after `start_playtest` or `step_playtest`, the project and
/// level after `open_project` or `create_level`, the build after a Build
/// command — with `ran` naming what was done. So a caller reads the
/// outcome of the call it made from its answer, instead of polling for
/// it. A call that left the editor nothing to do answers as it did.
[[nodiscard]] std::string agentHostOutcomeJson(const EditorShellState& state,
                                               const AgentResult& result);

}  // namespace eng::editor
