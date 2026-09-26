#pragma once

/// @file udp-listen.h
/// @brief A server's transport: ENet over UDP, listening on a port.
/// @par Threading
/// Main-thread-only, as every `NetTransport` is.

#include <cstdint>
#include <engine/net/net-transport.h>
#include <memory>
#include <optional>

namespace eng::net {

/// The port a session is hosted on when nobody says otherwise.
inline constexpr uint16_t UDP_DEFAULT_PORT = 47015;

/// A listening transport and the port it is bound to.
struct UdpListen {
  /// The transport: a peer per client that connects.
  std::unique_ptr<NetTransport> transport;
  /// The UDP port it listens on — the one asked for, or the one the
  /// system chose when asked for 0.
  uint16_t port = 0;
};

/// A server's transport on UDP port @p port of every local address, with
/// room for @p max_peers clients (ADR-013). Messages are ENet reliable
/// packets on one channel: whole, once, in order. A peer that stops
/// answering is disconnected within seconds. Nothing when the port cannot
/// be bound.
[[nodiscard]] std::optional<UdpListen> listenUdp(uint16_t port,
                                                 uint8_t max_peers);

}  // namespace eng::net
