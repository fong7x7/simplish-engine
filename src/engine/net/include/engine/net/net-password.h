#pragma once

/// @file net-password.h
/// @brief What a session password travels as.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <string_view>

namespace eng::net {

/// @p password as a `NetHello` carries it and a server compares it: a 64-bit
/// digest, 0 for no password — so an empty one is none.
///
/// A digest keeps the password itself off the wire, and nothing more: it
/// is unsalted and unencrypted, so anyone who can read the traffic can
/// replay it. It keeps strangers out of a co-op session; it is not
/// security (ADR-005: co-op trusts its peers).
[[nodiscard]] uint64_t netPasswordDigest(std::string_view password);

}  // namespace eng::net
