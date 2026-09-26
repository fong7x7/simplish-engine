#pragma once

/// @file net-refusal.h
/// @brief A server turning a client away.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::net {

/// Why a server would not seat a client.
enum class NetRefusalReason : uint8_t {
  PROTOCOL,  ///< The client speaks another `NET_PROTOCOL_VERSION`
  CONTENT,   ///< The client's content hash is not the server's
  FULL,      ///< Every seat is taken
};

/// Server to client, in answer to a `NetHello`, before disconnecting it.
struct NetRefusal {
  /// Why.
  NetRefusalReason reason = NetRefusalReason::FULL;

  /// Refusals are equal when their reasons are.
  bool operator==(const NetRefusal&) const = default;
};

}  // namespace eng::net
