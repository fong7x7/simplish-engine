#pragma once

/// @file net-message.h
/// @brief Any message of the lockstep protocol.
/// @par Threading
/// A value type.

#include <engine/net/net-desync.h>
#include <engine/net/net-end.h>
#include <engine/net/net-frame.h>
#include <engine/net/net-hash-report.h>
#include <engine/net/net-hello.h>
#include <engine/net/net-input.h>
#include <engine/net/net-refusal.h>
#include <engine/net/net-roster.h>
#include <engine/net/net-start.h>
#include <engine/net/net-trace.h>
#include <engine/net/net-waiting.h>
#include <engine/net/net-welcome.h>
#include <variant>

namespace eng::net {

/// One message of the protocol (ADR-013). The alternative's index is its
/// kind byte on the wire, so the order here is part of the protocol: add
/// at the end, and bump `NET_PROTOCOL_VERSION`.
using NetMessage = std::variant<NetHello, NetWelcome, NetRefusal, NetRoster,
                                NetStart, NetInput, NetFrame, NetHashReport,
                                NetDesync, NetEnd, NetWaiting, NetTrace>;

}  // namespace eng::net
