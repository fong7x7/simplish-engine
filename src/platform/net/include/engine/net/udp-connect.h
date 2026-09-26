#pragma once

/// @file udp-connect.h
/// @brief A client's transport: ENet over UDP, connecting to a server.
/// @par Threading
/// Main-thread-only, as every `NetTransport` is.

#include <cstdint>
#include <engine/net/net-transport.h>
#include <memory>
#include <string>

namespace eng::net {

/// A client's transport to the server at @p host — a name or a dotted
/// address — on UDP port @p port. It polls `CONNECTED` once the server
/// answers, or `DISCONNECTED` if it never does. Null when @p host does
/// not resolve.
[[nodiscard]] std::unique_ptr<NetTransport> connectUdp(const std::string& host,
                                                       uint16_t port);

}  // namespace eng::net
