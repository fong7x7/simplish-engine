#pragma once

/// @file udp-lan-game.h
/// @brief A session found on the local network, and asking for them.
/// @par Threading
/// Main-thread-only; `findLanGames` blocks for as long as it is told.

#include <chrono>
#include <cstdint>
#include <engine/net/net-lan-game.h>
#include <string>
#include <vector>

namespace eng::net {

/// Where a query goes when it is for the whole local network.
inline constexpr const char* UDP_LAN_BROADCAST = "255.255.255.255";

/// A server that answered a LAN query.
struct UdpLanGame {
  /// The address it answered from: where to connect, at `game.port`.
  std::string host;
  /// What it said of itself.
  NetLanGame game;
};

/// Ask @p address — `UDP_LAN_BROADCAST` for every machine on the network,
/// or one machine — on UDP port @p port who is hosting, and gather the
/// answers for @p wait. One per server, in the order they came.
///
/// For the whole network the query goes to each interface's own broadcast
/// address (on Windows only the limited broadcast, for now), the limited
/// broadcast, and 127.0.0.1 — a machine does not hear its own broadcasts,
/// and a session on it should still be found.
[[nodiscard]] std::vector<UdpLanGame>
findLanGames(const std::string& address, uint16_t port,
             std::chrono::milliseconds wait);

}  // namespace eng::net
