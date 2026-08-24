#pragma once

#include "epic-types.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace eng {

/// Parameters for sending a P2P packet via the Epic networking layer.
struct EpicPacketParams {
  /// Payload bytes to send.
  std::span<const std::byte> data{};
  /// Logical channel index for multiplexing traffic.
  uint8_t channel = 0;
  /// Delivery guarantee mode for this packet.
  EpicP2PReliability reliability = EpicP2PReliability::RELIABLE_ORDERED;
};

}  // namespace eng
