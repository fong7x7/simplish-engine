#pragma once

/// @file measure-limit.h
/// @brief What a widget is measured with.
/// @par Threading
/// Main thread only.

#include <engine/gui/gui-draw-context.h>

namespace eng {

/// What a widget is measured with: the text it measures in, and how wide
/// it may be.
struct MeasureLimit {
  /// Draw context for measuring text.
  const GuiDrawContext& ctx;
  /// Most the widget's border box may be across; negative for no limit.
  float max_width = -1.0f;
};

}  // namespace eng
