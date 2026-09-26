#pragma once

/// @file net-codec.h
/// @brief Messages to bytes and back.
/// @par Threading
/// Pure functions.

#include <cstddef>
#include <engine/net/net-message.h>
#include <optional>
#include <span>
#include <vector>

namespace eng::net {

/// @p message as the bytes that carry it: its kind as one byte, then its
/// fields — fixed-width integers little-endian, counts and ticks as
/// varints, stick axes zigzagged — the same on every host.
[[nodiscard]] std::vector<std::byte>
encodeNetMessage(const NetMessage& message);

/// The message @p bytes carry, or nothing when they are not exactly one
/// well-formed message. Hostile-input safe: bytes come from other
/// machines, so nothing is read past the end, every count is bounded, and
/// trailing bytes are refused.
[[nodiscard]] std::optional<NetMessage>
decodeNetMessage(std::span<const std::byte> bytes);

}  // namespace eng::net
