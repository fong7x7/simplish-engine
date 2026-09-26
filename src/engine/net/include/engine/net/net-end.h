#pragma once

/// @file net-end.h
/// @brief A server ending a run.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::net {

/// Server to every client in a run: there are no frames after the ones
/// already sent. Clients stay seated for the next start.
struct NetEnd {
  /// The run ending.
  uint16_t run = 0;

  /// Ends are equal when their runs are.
  bool operator==(const NetEnd&) const = default;
};

}  // namespace eng::net
