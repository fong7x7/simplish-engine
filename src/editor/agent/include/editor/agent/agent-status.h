#pragma once

/// @file agent-status.h
/// @brief How a tool call turned out.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// The outcome of one tool call.
///
/// Failure is in the return value, not in an exception
/// ([ADR-001](../../../../../docs/decisions/ADR-001-no-exceptions.md)), and
/// the distinctions here are the ones a caller can act on: a name it should
/// stop using, a parameter it should fix, an index it should re-read, and a
/// state it should wait out or change first.
/// @thread_safety Immutable value type.
enum class AgentStatus : uint8_t {
  /// The tool ran. Its payload is the answer.
  OK,
  /// No tool is called that. The caller's tool list is stale; `describe`
  /// reports the current one.
  UNKNOWN_TOOL,
  /// A parameter is missing, of the wrong sort, or not one of the words
  /// the schema allows.
  BAD_PARAMS,
  /// The tool is right and the parameters are well formed, but what they
  /// name is not there: an index past the end of a list, an asset no scan
  /// found, a project directory that holds no project.
  NOT_FOUND,
  /// The editor cannot do this right now: undo with nothing to undo, a
  /// menu command the editor lists as disabled, an edit with no project
  /// open.
  UNAVAILABLE,
};

/// The word this status is reported as, which is what a caller matches on.
[[nodiscard]] constexpr std::string_view agentStatusName(AgentStatus status) {
  switch (status) {
    case AgentStatus::OK:
      return "ok";
    case AgentStatus::UNKNOWN_TOOL:
      return "unknown_tool";
    case AgentStatus::BAD_PARAMS:
      return "bad_params";
    case AgentStatus::NOT_FOUND:
      return "not_found";
    case AgentStatus::UNAVAILABLE:
      return "unavailable";
  }
  return "unknown_tool";
}

}  // namespace eng::editor
