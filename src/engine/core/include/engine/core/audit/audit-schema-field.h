#pragma once

#include "audit-types.h"

#include <cstdint>
#include <string>

namespace eng {

struct AuditSchemaField {
  /// Field name used in payload and display.
  std::string name;
  /// Data type of this field.
  AuditFieldType type;
  /// Fixed byte size of the field (0 for variable-length types like string16).
  uint16_t fixed_size{};
};

}  // namespace eng
