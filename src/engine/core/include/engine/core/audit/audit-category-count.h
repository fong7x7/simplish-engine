#pragma once

#include <cstdint>

namespace eng {

struct AuditCategoryCount {
  /// Event category ID.
  uint16_t category{};
  /// Number of events in this category.
  uint64_t count{};
};

}  // namespace eng
