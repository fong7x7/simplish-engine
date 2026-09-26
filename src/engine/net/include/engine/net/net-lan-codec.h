#pragma once

/// @file net-lan-codec.h
/// @brief LAN discovery's two datagrams: the query, and a server's answer.
/// @par Threading
/// Pure functions.

#include <cstddef>
#include <engine/net/net-lan-game.h>
#include <optional>
#include <span>
#include <vector>

namespace eng::net {

/// The query a player broadcasts to find servers: `SMPL` and `Q`. Not a
/// session message — nobody is connected — so it has its own framing.
[[nodiscard]] std::vector<std::byte> encodeLanQuery();

/// Whether @p bytes are exactly a query.
[[nodiscard]] bool isLanQuery(std::span<const std::byte> bytes);

/// @p game as the datagram that answers a query: `SMPL`, `G`, then its
/// fields, little-endian, then the name and the session number.
[[nodiscard]] std::vector<std::byte> encodeLanGame(const NetLanGame& game);

/// The answer @p bytes carry, or nothing when they are not exactly one.
/// Hostile-input safe: any machine on the network can send anything.
[[nodiscard]] std::optional<NetLanGame>
decodeLanGame(std::span<const std::byte> bytes);

}  // namespace eng::net
