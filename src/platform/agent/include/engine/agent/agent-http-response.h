#pragma once

/// @file agent-http-response.h
/// @brief What the agent API sends back.
/// @par Threading Main-thread-only.

#include <engine/agent/agent-reply-timing.h>
#include <string>

namespace eng::agent {

/// One response: a status and a JSON body.
///
/// The content type is always `application/json`, because everything this
/// server serves is JSON. A handler with something else to say says it in
/// the body.
/// @thread_safety Main-thread-only.
struct AgentHttpResponse {
  /// HTTP status. 200 for a request that was understood, whatever the tool
  /// inside it made of the call — a tool that refuses says so in its own
  /// payload, where a caller reading the JSON will see it.
  int status = 200;
  /// The JSON body.
  std::string body = "{}";
  /// Whether this is the answer, or the server should ask again next poll.
  AgentReplyTiming timing = AgentReplyTiming::NOW;
};

}  // namespace eng::agent
