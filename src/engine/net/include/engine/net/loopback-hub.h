#pragma once

/// @file loopback-hub.h
/// @brief What a `LoopbackNetwork` and its transports share.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <deque>
#include <engine/net/net-event.h>
#include <vector>

namespace eng::net {

/// The peer number a loopback client knows its server by.
inline constexpr NetPeer LOOPBACK_SERVER_PEER = 0;

/// The loopback network's state: an inbox per endpoint, and which clients
/// are connected to the listener. Endpoint 0 is the listener; each
/// `connect` adds one, numbered as the listener's peer for it.
/// Shared by the network and every transport it made, so either may be
/// destroyed first.
struct LoopbackHub {
  /// Events waiting for each endpoint, oldest first.
  std::vector<std::deque<NetEvent>> inboxes =
      std::vector<std::deque<NetEvent>>(1);
  /// Per endpoint, 1 while its connection to the listener is open.
  std::vector<uint8_t> open = std::vector<uint8_t>(1);
  /// 1 while a listener exists.
  uint8_t listening = 0;
  /// The round trip every connection reports, in milliseconds.
  uint32_t round_trip_ms = 0;
};

}  // namespace eng::net
