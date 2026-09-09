#pragma once

/// @file agent-connection.h
/// @brief One client the agent server is part-way through reading.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <string>

namespace eng::agent {

/// The socket value standing for "no socket".
///
/// Sockets are held as `intptr_t` rather than as the platform's own type,
/// because that type is an `int` on one family of platforms and an opaque
/// handle on the other, and the server itself never does anything with the
/// value but pass it back.
inline constexpr intptr_t AGENT_SOCKET_NONE = -1;

/// A connected client and what has arrived from it so far.
///
/// The server reads whatever a poll makes available and keeps it here
/// until a whole request has landed. One request per connection: the
/// response closes it, which is what `Connection: close` promises.
/// @thread_safety Main-thread-only.
struct AgentConnection {
  /// The accepted socket, or `AGENT_SOCKET_NONE` once it is closed.
  intptr_t socket = AGENT_SOCKET_NONE;
  /// Bytes read from it and not yet parsed.
  std::string buffer;
};

}  // namespace eng::agent
