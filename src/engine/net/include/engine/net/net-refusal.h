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
  BUILD,     ///< The client was built from other code than the server
  PASSWORD,  ///< The client's password is not the session's
  STALLED,   ///< Seated, it stopped sending input and was dropped
};

/// Server to client, before disconnecting it: in answer to a `NetHello`,
/// or — `STALLED` — to a seat the session gave up waiting for.
struct NetRefusal {
  /// Why.
  NetRefusalReason reason = NetRefusalReason::FULL;

  /// Refusals are equal when their reasons are.
  bool operator==(const NetRefusal&) const = default;
};

}  // namespace eng::net
