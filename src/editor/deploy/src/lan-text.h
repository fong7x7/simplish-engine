#pragma once

/// @file lan-text.h
/// @brief Sessions found on the LAN, for a person to read and pick from.
/// @par Threading Pure functions.

#include <cstdint>
#include <engine/net/udp-lan-game.h>
#include <optional>
#include <span>
#include <string>

namespace eng::editor {

/// Whether a client built as @p build, with content hashed @p content,
/// would be let into @p found — its password aside.
[[nodiscard]] bool lanGameCompatible(const net::UdpLanGame& found,
                                     uint64_t build, uint64_t content);

/// @p found in a line: where, what, who is in, and what stands in the way
/// of joining it for a client built as @p build with content @p content.
[[nodiscard]] std::string lanGameText(const net::UdpLanGame& found,
                                      uint64_t build, uint64_t content);

/// The first of @p games that a client built as @p build with content
/// @p content can join now: compatible, not full, and waiting for players
/// rather than mid-run.
[[nodiscard]] std::optional<net::UdpLanGame>
pickLanGame(std::span<const net::UdpLanGame> games, uint64_t build,
            uint64_t content);

}  // namespace eng::editor
