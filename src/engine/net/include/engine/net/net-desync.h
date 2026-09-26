#pragma once

/// @file net-desync.h
/// @brief Two peers disagreeing on the state at a tick.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::net {

/// The `slot` a server's own hash reports are made from.
inline constexpr uint8_t NET_SERVER_SLOT = 0xFF;

/// The `section` of a desync whose hashes disagree on how many sections
/// there are — two different builds, rather than one run diverging.
inline constexpr uint8_t NET_SECTION_COUNT_DIFFERS = 0xFF;

/// Server to every client in a run: the run has diverged and is halted
/// (ADR-005). Each peer names the section from its own hash's names.
struct NetDesync {
  /// The run it is for.
  uint16_t run = 0;
  /// The tick the hashes were taken at.
  uint64_t tick = 0;
  /// The first section that differs, by index, or
  /// `NET_SECTION_COUNT_DIFFERS`.
  uint8_t section = 0;
  /// The seat whose report disagreed with the first one taken, or
  /// `NET_SERVER_SLOT`.
  uint8_t slot = 0;

  /// Desyncs are equal when every field is.
  bool operator==(const NetDesync&) const = default;
};

}  // namespace eng::net
