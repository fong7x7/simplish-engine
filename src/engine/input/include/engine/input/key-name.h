#pragma once

/// @file key-name.h
/// @brief A name a binding file can give a key.
/// @par Threading
/// A value type.

#include <cstdint>
#include <string_view>

namespace eng::input {

/// One key a binding file names in words — "up", "space" — rather than by
/// its symbol. The platform that owns key symbols supplies the table; the
/// engine only reads it.
struct KeyName {
  /// The word the file uses, lowercase.
  std::string_view name;
  /// The platform key symbol it stands for.
  uint32_t key = 0;
};

}  // namespace eng::input
