#pragma once

/// @file agent-param.h
/// @brief One parameter in a tool's published schema.
/// @par Threading Thread-safe (immutable value type).

#include <editor/agent/agent-param-need.h>
#include <editor/agent/agent-param-type.h>
#include <string_view>

namespace eng::editor {

/// One named input to a tool.
///
/// The editor publishes these and the MCP bridge turns them into the JSON
/// Schema an agent reads, so this table is the single description of what a
/// tool takes — there is no second copy on the bridge side to drift from
/// it.
/// @thread_safety Immutable value type.
struct AgentParam {
  /// Key this parameter is passed under.
  std::string_view name;
  /// What sort of value it takes.
  AgentParamType type = AgentParamType::STRING;
  /// Whether a call without it is rejected.
  AgentParamNeed need = AgentParamNeed::REQUIRED;
  /// One line an agent can act on: what the value means, what the accepted
  /// words are, and what an omitted optional one defaults to.
  std::string_view description;
};

}  // namespace eng::editor
