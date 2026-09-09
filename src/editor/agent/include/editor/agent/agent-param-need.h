#pragma once

/// @file agent-param-need.h
/// @brief Whether a tool parameter has to be given.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// Whether a call must carry a parameter.
///
/// A named pair rather than a bare `bool` member, so the schema table below
/// reads as what it means at every row rather than as a column of `true`
/// and `false`.
/// @thread_safety Immutable value type.
enum class AgentParamNeed : uint8_t {
  /// The call is rejected without it.
  REQUIRED,
  /// The tool has a documented default for it.
  OPTIONAL,
};

}  // namespace eng::editor
