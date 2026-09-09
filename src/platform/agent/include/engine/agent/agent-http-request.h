#pragma once

/// @file agent-http-request.h
/// @brief One HTTP request read off the agent API's socket.
/// @par Threading Main-thread-only.

#include <string>

namespace eng::agent {

/// A request, reduced to the three things the agent API routes on.
///
/// Headers other than `Content-Length` are read and dropped. This serves
/// one caller on the loopback interface — an agent, or somebody with curl —
/// and every header it would act on is one more thing that can lie to it.
/// @thread_safety Main-thread-only.
struct AgentHttpRequest {
  /// The method as the client wrote it: `GET` or `POST`.
  std::string method;
  /// The path, with any query string already cut off.
  std::string path;
  /// The body, empty for a GET.
  std::string body;
};

}  // namespace eng::agent
