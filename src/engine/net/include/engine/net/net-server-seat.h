#pragma once

/// @file net-server-seat.h
/// @brief One seat at a lockstep server's table.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/net/net-event.h>
#include <string>

namespace eng::net {

/// A seat at the server's table.
struct NetServerSeat {
  /// The client's connection; meaningful while `connected`.
  NetPeer peer = 0;
  /// 1 while a client sits here.
  uint8_t connected = 0;
  /// 1 while the client is playing the current run — seated when it
  /// started, and not dropped since.
  uint8_t playing = 0;
  /// The tick after the last input the client sent this run.
  uint64_t next_input = 0;
  /// Who the client asked to play as.
  std::string character;
};

}  // namespace eng::net
