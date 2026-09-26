#pragma once

/// @file net-hello.h
/// @brief A client asking a server for a seat.
/// @par Threading
/// A value type.

#include <cstdint>
#include <string>

namespace eng::net {

/// The protocol a build speaks. A server refuses a client speaking another.
/// Bump it with any change to a message's bytes.
inline constexpr uint16_t NET_PROTOCOL_VERSION = 1;

/// Longest level or character id a message may carry, in bytes.
inline constexpr uint64_t NET_MAX_ID_BYTES = 256;

/// Client to server, first: who is asking, and what they are running.
struct NetHello {
  /// `NET_PROTOCOL_VERSION` as the client was built.
  uint16_t protocol = NET_PROTOCOL_VERSION;
  /// A hash of the content the client simulates with. Two peers with
  /// different content simulate different runs, so a server refuses a
  /// client whose hash is not its own — before the first tick rather than
  /// at the first desync.
  uint64_t content_hash = 0;
  /// Who the player wants to play as: a character id the game resolves, or
  /// empty for the default. At most `NET_MAX_ID_BYTES`.
  std::string character;

  /// Hellos are equal when every field is.
  bool operator==(const NetHello&) const = default;
};

}  // namespace eng::net
