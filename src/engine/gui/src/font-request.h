#pragma once

/// @file font-request.h
/// @brief The weight and slant a font is wanted at.
/// @par Threading
/// Immutable value type.

#include <cstdint>
#include <engine/gui/font-face.h>
#include <engine/gui/text-pipeline.h>

namespace eng {

/// A weight and slant asked for.
struct FontRequest {
  /// CSS weight, 100–900.
  uint16_t weight = FONT_WEIGHT_NORMAL;
  /// Upright or italic.
  FontLoadItalic italic = FontLoadItalic::NORMAL;
};

}  // namespace eng
