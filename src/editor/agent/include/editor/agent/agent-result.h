#pragma once

/// @file agent-result.h
/// @brief What one tool call produced.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-host-request.h>
#include <editor/agent/agent-status.h>
#include <engine/agent/agent-reply-timing.h>
#include <string>

namespace eng::editor {

/// The outcome of running one tool.
/// @thread_safety Main-thread-only.
struct AgentResult {
  /// How the call turned out.
  AgentStatus status = AgentStatus::OK;
  /// The answer, as a JSON value. An object for every tool that succeeds;
  /// on a failure it is a message string saying what was wrong, in terms
  /// of the call that was made.
  std::string json = "{}";
  /// What the editor still has to carry out, or a `NONE` request.
  AgentHostRequest host;
  /// Whether the call changed the editor's state, and the chrome therefore
  /// has to be rebuilt from it. False for every read, and false for a
  /// write that turned out to change nothing.
  bool changed = false;
  /// Whether this is the answer, or the caller is waiting for something —
  /// a build to finish, a playtest to reach a tick — and the call is to be
  /// asked again on the next frame.
  agent::AgentReplyTiming timing = agent::AgentReplyTiming::NOW;
};

}  // namespace eng::editor
