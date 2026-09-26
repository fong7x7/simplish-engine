#pragma once

/// @file net-lan-game.h
/// @brief A server's answer to "who is hosting on this network?".
/// @par Threading
/// A value type.

#include <cstdint>
#include <string>

namespace eng::net {

/// The UDP port servers answer LAN queries on when nobody says otherwise:
/// the one after `UDP_DEFAULT_PORT`.
inline constexpr uint16_t NET_LAN_PORT = 47016;

/// Longest game name an answer carries, in bytes.
inline constexpr uint64_t NET_MAX_LAN_NAME_BYTES = 64;

/// What a server says of itself to a LAN query: enough for a player to
/// pick a session and to know beforehand whether it would be let in.
struct NetLanGame {
  /// The server's `NET_PROTOCOL_VERSION`.
  uint16_t protocol = 0;
  /// The server's build id; a client with another is refused.
  uint64_t build = 0;
  /// The server's content hash; a client with another is refused.
  uint64_t content_hash = 0;
  /// The UDP port the session itself is on.
  uint16_t port = 0;
  /// Seats with a player in them.
  uint8_t seated = 0;
  /// Seats in all.
  uint8_t seats = 0;
  /// 1 while a run is on: a player who joins waits for the next one.
  uint8_t running = 0;
  /// 1 when the session wants a password.
  uint8_t locked = 0;
  /// What the host calls the game. At most `NET_MAX_LAN_NAME_BYTES`.
  std::string name;
  /// A number the server picked at random when it began answering: one
  /// server heard at two addresses — its own machine's loopback and its
  /// network interface — is the same session, and listed once.
  uint64_t session = 0;

  /// Answers are equal when every field is.
  bool operator==(const NetLanGame&) const = default;
};

}  // namespace eng::net
