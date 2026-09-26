#pragma once

/// @file net-event.h
/// @brief Something a transport has to report: a peer came, spoke or went.
/// @par Threading
/// A value type.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eng::net {

/// Which connection an event or a send is about. A transport numbers its
/// own peers; the number means nothing to any other transport.
using NetPeer = uint32_t;

/// What happened to a peer.
enum class NetEventKind : uint8_t {
  CONNECTED,     ///< The connection is open and messages may be sent on it
  RECEIVED,      ///< A whole message arrived from the peer
  DISCONNECTED,  ///< The connection is closed, or never opened
};

/// One event from `NetTransport::poll`.
struct NetEvent {
  /// What happened.
  NetEventKind kind = NetEventKind::CONNECTED;
  /// The peer it happened to.
  NetPeer peer = 0;
  /// The message, when `kind` is `RECEIVED`; otherwise empty.
  std::vector<std::byte> bytes;
};

}  // namespace eng::net
