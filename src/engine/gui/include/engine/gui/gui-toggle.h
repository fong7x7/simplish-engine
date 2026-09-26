#pragma once

/// @file gui-toggle.h
/// @brief An on/off switch with a label.
/// @par Threading
/// Main thread only.

#include "gui-widget.h"

#include <functional>
#include <string>

namespace eng {

/// A switch: a pill whose knob slides across as it turns on, the track
/// taking the accent — for settings that apply at once. Its label is to
/// its left; the whole row is the target. Takes focus.
/// @thread_safety Main thread only.
class GuiToggle : public GuiWidget {
public:
  /// An off, focusable switch.
  GuiToggle();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Slide the knob towards where it belongs.
  void update(const GuiDrawContext& ctx, float dt) override;

  /// Its label and switch.
  void render(const GuiDrawContext& ctx) const override;

  /// The label, a gap and the switch, one line or the switch high.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// Flip it.
  bool handleClick(const GuiMouseEvent& event) override;

  /// CONFIRM flips it; LEFT turns it off and RIGHT on.
  bool handleNav(GuiNavCommand command) override;

  /// Turn it over and tell `on_change`.
  void flip();

  /// To its left.
  std::string label{};
  /// Whether it is on.
  bool on = false;
  /// Called with the new value after it changes.
  std::function<void(bool)> on_change{};

private:
  /// Draw the pill and its knob in @p track.
  void drawSwitch(const GuiDrawContext& ctx, const Rect& track) const;

  /// Where the knob is: 0 off to 1 on, easing after `on`; negative until
  /// the first `update`, which starts it where `on` is.
  float knob_ = -1.0f;

  /// Where the knob is drawn: `knob_`, or where `on` is before any
  /// `update` — so a toggle never updated, in a capture, draws its state.
  [[nodiscard]] float knob() const;
};

}  // namespace eng
