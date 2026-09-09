#pragma once

/// @file local-agent-server.h
/// @brief A loopback HTTP server the editor pumps from its own frame loop.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <engine/agent/agent-connection.h>
#include <engine/agent/agent-http-request.h>
#include <engine/agent/agent-http-response.h>
#include <functional>
#include <vector>

namespace eng::agent {

/// A tiny HTTP server, bound to the loopback interface, driven by polling.
///
/// Single-threaded on purpose. Every request it accepts ends up editing the
/// editor's document, which is main-thread-only state; a threaded server
/// would have to hand the work back to the main thread anyway, and would
/// add a queue, a lock, and a class of bug that the frame loop calling
/// `poll` once a tick does not have. The cost is that a call waits up to
/// one frame, which at sixty frames a second is not a cost.
///
/// It binds 127.0.0.1 and nothing else, so nothing off this machine can
/// reach it. That is the whole of its access control, which is why the
/// editor does not open it unless it is asked to.
/// @thread_safety Main-thread-only.
class LocalAgentServer {
public:
  LocalAgentServer() = default;
  ~LocalAgentServer();
  LocalAgentServer(const LocalAgentServer&) = delete;
  LocalAgentServer& operator=(const LocalAgentServer&) = delete;
  LocalAgentServer(LocalAgentServer&&) = delete;
  LocalAgentServer& operator=(LocalAgentServer&&) = delete;

  /// Bind @p port on the loopback interface and start listening. False
  /// when the port is taken or sockets are unavailable, which leaves the
  /// editor running with no agent API rather than not running.
  bool open(uint16_t port);

  /// Stop listening and drop every connection.
  void close();

  /// Whether the server is listening.
  [[nodiscard]] bool isOpen() const;

  /// The port it is listening on, or zero when it is not.
  [[nodiscard]] uint16_t port() const;

  /// Accept what is waiting, read what has arrived, and answer every
  /// request that is now complete through @p handler.
  ///
  /// Never blocks. A connection that has sent half a request is left
  /// half-read until the next poll.
  void poll(
      const std::function<AgentHttpResponse(const AgentHttpRequest&)>& handler);

private:
  /// Accept every connection waiting on the listening socket.
  void acceptPending();
  /// Read from @p connection, and answer it if a whole request has landed.
  /// False when the connection is finished with and should be dropped.
  bool serve(
      AgentConnection& connection,
      const std::function<AgentHttpResponse(const AgentHttpRequest&)>& handler);
  /// Answer the complete request of @p length at the front of the buffer.
  bool respond(
      AgentConnection& connection, size_t length,
      const std::function<AgentHttpResponse(const AgentHttpRequest&)>& handler);

  /// The listening socket, or `AGENT_SOCKET_NONE` when closed.
  intptr_t listener_ = AGENT_SOCKET_NONE;
  /// The port `open` bound, or zero.
  uint16_t port_ = 0;
  /// Clients part-way through a request.
  std::vector<AgentConnection> connections_;
};

}  // namespace eng::agent
