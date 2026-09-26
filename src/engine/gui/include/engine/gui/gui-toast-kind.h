#pragma once

/// @file gui-toast-kind.h
/// @brief What a toast is telling the user.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// The kind of notice a toast carries, which colours its accent.
enum class GuiToastKind : uint8_t {
  /// Something happened: saved, loaded.
  INFO,
  /// Something worked: a build succeeded.
  SUCCESS,
  /// Something needs a look.
  WARNING,
  /// Something failed.
  ERROR,
};

}  // namespace eng
