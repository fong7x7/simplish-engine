#pragma once

#include <cstdint>

namespace eng {

/// Binary flag value for scripting / plugin mutation APIs (replaces bool
/// params).
enum class VxFlagBinary : uint8_t {
  OFF,
  ON,
};

}  // namespace eng
