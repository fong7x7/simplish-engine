#pragma once

#include "gui-slider-style.h"
#include "gui-widget.h"

#include <functional>

namespace eng {

/// A horizontal slider for numeric value selection.
/// Renders a track with a filled portion and a draggable handle.
/// Value is stored normalized [0, 1] and mapped to [min_value, max_value].
/// @thread_safety Main thread only.
class GuiSlider : public GuiWidget {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render the slider (track + fill + handle).
  void render(const GuiDrawContext& ctx) const override;

  /// Capture mouse on track/handle click and update value.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Update value while dragging.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Release capture.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// Resolve track/fill/handle colors from shared or per-instance style.
  struct ResolvedColors {
    /// Track background color.
    GuiColor track;
    /// Filled portion color.
    GuiColor fill;
    /// Handle color (accounts for hover state).
    GuiColor handle;
    /// Track bar height in logical pixels.
    float track_height{};
    /// Handle size in logical pixels.
    float handle_size{};
  };

  /// Resolve style colors and dimensions for rendering.
  ResolvedColors resolveStyle() const;

  /// Return the value mapped to [min_value, max_value].
  float mappedValue() const;

  /// Set value from a mapped value, clamping to [min_value, max_value].
  void setMappedValue(float mapped);

  /// Normalized value in [0, 1].
  float value = 0.0f;
  /// Minimum of the output range.
  float min_value = 0.0f;
  /// Maximum of the output range.
  float max_value = 1.0f;
  /// Visual styling.
  GuiSliderStyle style{};
  /// Callback fired with the mapped value when the slider changes.
  std::function<void(float)> on_change{};

private:
  /// Update normalized value from mouse x position and fire on_change.
  void updateValueFromX(float mx);
};

}  // namespace eng
