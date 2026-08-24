#pragma once

#include <cstdint>

namespace eng::render {

/// Number of frames buffered in flight for DX12 (double-buffered).
inline constexpr uint32_t DX12_FRAMES_IN_FLIGHT = 2;

}  // namespace eng::render
