#pragma once

/// @file udp-lan-beacon.h
/// @brief A server answering LAN queries, so players can find its session.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/net/net-lan-game.h>
#include <memory>

namespace eng::net {

/// The socket value standing for "no socket". Held as an integer rather
/// than the platform's own type, which is an `int` on one family of
/// platforms and an opaque handle on the other.
inline constexpr intptr_t UDP_LAN_NO_SOCKET = -1;

/// A UDP socket a server keeps open beside its session, answering every
/// `encodeLanQuery` that reaches it — broadcast to the whole network, or
/// sent to it directly — with what the server says of itself. Bound with
/// address reuse, so several servers on one machine can each answer.
class UdpLanBeacon {
public:
  /// A beacon on UDP port @p port of every local address; 0 for any free
  /// port. Null when the port cannot be had.
  [[nodiscard]] static std::unique_ptr<UdpLanBeacon> open(uint16_t port);

  /// A beacon over @p socket, bound to @p port, which it owns.
  UdpLanBeacon(intptr_t socket, uint16_t port);
  /// Closes the socket.
  ~UdpLanBeacon();
  UdpLanBeacon(const UdpLanBeacon&) = delete;
  UdpLanBeacon& operator=(const UdpLanBeacon&) = delete;
  UdpLanBeacon(UdpLanBeacon&&) = delete;
  UdpLanBeacon& operator=(UdpLanBeacon&&) = delete;

  /// Answer every query waiting with @p game; ignore anything else.
  /// Never blocks.
  void answer(const NetLanGame& game) const;

  /// The port it is bound to.
  [[nodiscard]] uint16_t port() const { return port_; }

private:
  /// The socket.
  intptr_t socket_;
  /// The port it is bound to.
  uint16_t port_;
};

}  // namespace eng::net
