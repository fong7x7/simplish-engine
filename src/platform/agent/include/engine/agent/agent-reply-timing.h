#pragma once

/// @file agent-reply-timing.h
/// @brief Whether a handler's answer is ready, or should be asked again.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::agent {

/// When a request's answer goes back.
enum class AgentReplyTiming : uint8_t {
  /// Now: this response is the answer.
  NOW,
  /// Not yet: the server keeps the connection open and hands the same
  /// request to the handler again on the next poll, until it answers.
  /// How a caller waits for something — a build — without the editor
  /// stopping to wait with it.
  LATER,
};

}  // namespace eng::agent
