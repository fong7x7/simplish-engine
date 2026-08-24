#pragma once

#include <cstdint>

namespace eng {

/// @thread_safety Main thread only.
enum class TextStyle : uint8_t {
  NORMAL,
  BOLD,
  ITALIC,
  BOLD_ITALIC,
  CODE,
};

}  // namespace eng
