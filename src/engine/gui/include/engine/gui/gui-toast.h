#pragma once

/// @file gui-toast.h
/// @brief One brief notice in a toast stack.
/// @par Threading
/// Main thread only.

#include "gui-toast-kind.h"

#include <string>

namespace eng {

/// A notice `GuiToasts` is showing.
struct GuiToast {
  /// What it says.
  std::string text{};
  /// What kind of notice it is.
  GuiToastKind kind = GuiToastKind::INFO;
  /// Seconds it has been shown.
  float age = 0.0f;
  /// Seconds it shows for, fading in and out included.
  float lifetime = 4.0f;
};

}  // namespace eng
