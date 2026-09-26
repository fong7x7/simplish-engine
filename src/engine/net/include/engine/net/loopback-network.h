#pragma once

/// @file loopback-network.h
/// @brief A network inside one process: transports whose messages are queues.
/// @par Threading
/// Main-thread-only, and every transport it makes with it.

#include <engine/net/loopback-hub.h>
#include <engine/net/net-transport.h>
#include <memory>

namespace eng::net {

/// A server and its clients in one process, with no sockets: what the
/// engine's tests run sessions over, and what a single-process session
/// (a server and its own client) may use (ADR-013). A sent message is
/// delivered on the receiver's next poll; nothing is ever lost.
class LoopbackNetwork {
public:
  /// The listening end — a server's transport. Null when this network
  /// already has one.
  [[nodiscard]] std::unique_ptr<NetTransport> listen();

  /// A client's transport, connecting to the listener. It polls
  /// `CONNECTED` when there is one to connect to, `DISCONNECTED` when not.
  [[nodiscard]] std::unique_ptr<NetTransport> connect();

  /// Have every connection report a round trip of @p ms — a network that
  /// delivers at once, pretending to be a slower one, for a test of what
  /// a server makes of a measurement.
  void setRoundTrip(uint32_t ms) { hub_->round_trip_ms = ms; }

private:
  /// The queues, shared with every transport made here.
  std::shared_ptr<LoopbackHub> hub_ = std::make_shared<LoopbackHub>();
};

}  // namespace eng::net
