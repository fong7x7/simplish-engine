#pragma once

/// @file gui-modal-dismiss.h
/// @brief What closes a modal without an answer.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// How a modal can be closed without choosing anything in it.
enum class GuiModalDismiss : uint8_t {
  /// A click on the dimmed backdrop, or CANCEL (Escape, a pad's back).
  BACKDROP_OR_CANCEL,
  /// CANCEL only: a stray click must not lose the question.
  CANCEL_ONLY,
  /// Nothing: only its own buttons close it.
  NEVER,
};

}  // namespace eng
