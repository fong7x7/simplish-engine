#pragma once

/// @file gui-toasts.h
/// @brief Brief notices that stack in a corner and fade away.
/// @par Threading
/// Main thread only.

#include "gui-toast.h"
#include "gui-widget.h"

#include <string>
#include <vector>

namespace eng {

/// Toasts: short notices — "Saved", "Build failed" — stacked in the
/// bottom-right corner of its rect, newest at the bottom, each fading in,
/// staying a few seconds, and fading out. Covers its parent, the tree's
/// overlay layer, and lets the pointer through, so it never blocks what is
/// under it. Draws its toasts itself: showing one needs no layout.
///
/// ```cpp
/// auto& toasts = *dynamic_cast<GuiToasts*>(tree.findWidget(
///     tree.insertExternalWidget(std::make_unique<GuiToasts>(),
///                               tree.overlayLayer())));
/// toasts.show("Level saved");
/// toasts.show("Build failed: 3 errors", GuiToastKind::ERROR, 8.0f);
/// ```
/// @thread_safety Main thread only.
class GuiToasts : public GuiWidget {
public:
  /// A toast stack covering its parent, letting the pointer through.
  GuiToasts();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Age the toasts and drop the ones that have run their time.
  void update(const GuiDrawContext& ctx, float dt) override;

  /// Draw the toasts, bottom-right, newest lowest.
  void render(const GuiDrawContext& ctx) const override;

  /// Show @p text as a @p kind notice for @p seconds. Beyond
  /// `max_shown`, the oldest goes.
  void show(std::string text, GuiToastKind kind = GuiToastKind::INFO,
            float seconds = 4.0f);

  /// The toasts showing, oldest first.
  [[nodiscard]] const std::vector<GuiToast>& shown() const;

  /// Most toasts shown at once.
  size_t max_shown = 4;
  /// Widest a toast grows before its text is cut short.
  float max_width = 360.0f;

private:
  /// The toasts showing, oldest first.
  std::vector<GuiToast> toasts_{};
};

}  // namespace eng
