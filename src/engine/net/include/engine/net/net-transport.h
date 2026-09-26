#pragma once

/// @file net-transport.h
/// @brief Whole messages to and from numbered peers, reliably and in order.
/// @par Threading
/// Main-thread-only. Nothing happens between polls.

#include <cstddef>
#include <engine/net/net-event.h>
#include <optional>
#include <span>

namespace eng::net {

/// What a lockstep session is carried by (ADR-013). Messages arrive whole,
/// exactly once, and in the order each peer sent them — the guarantees
/// lockstep needs and nothing more. A server's transport has a peer per
/// client; a client's has one, the server.
///
/// UDP through ENet is in `platform/net`; `LoopbackNetwork` carries a
/// session inside one process; a distributor relay is one more of these
/// (Platform §4.3, PLT-DST-5). Nothing above this interface knows which.
class NetTransport {
public:
  NetTransport() = default;
  virtual ~NetTransport() = default;
  NetTransport(const NetTransport&) = delete;
  NetTransport& operator=(const NetTransport&) = delete;
  NetTransport(NetTransport&&) = delete;
  NetTransport& operator=(NetTransport&&) = delete;

  /// The next thing that happened, oldest first; nothing when nothing has.
  /// Sends and receives only make progress while this is being called.
  virtual std::optional<NetEvent> poll() = 0;

  /// Queues @p bytes to @p peer. A peer that is not connected is ignored:
  /// its `DISCONNECTED` event is, or will be, in the poll queue.
  virtual void send(NetPeer peer, std::span<const std::byte> bytes) = 0;

  /// Closes the connection to @p peer once what was sent to it has gone.
  /// Both sides then poll a `DISCONNECTED` for it.
  virtual void disconnect(NetPeer peer) = 0;
};

}  // namespace eng::net
