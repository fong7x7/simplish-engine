#pragma once

/// @file gui-radio-group.h
/// @brief A set of options, exactly one chosen.
/// @par Threading
/// Main thread only.

#include "gui-widget.h"

#include <functional>
#include <string>
#include <vector>

namespace eng {

/// Radio buttons: its options in a column, each a circle and a label, one
/// chosen at a time. A click chooses; UP and DOWN move the choice while it
/// has focus. Takes focus as one widget.
/// @thread_safety Main thread only.
class GuiRadioGroup : public GuiWidget {
public:
  /// An empty, focusable group.
  GuiRadioGroup();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Its options.
  void render(const GuiDrawContext& ctx) const override;

  /// As wide as its widest option, a row per option.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// Choose the option under the pointer.
  bool handleClick(const GuiMouseEvent& event) override;

  /// Light the option under the pointer.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// UP and DOWN move the choice.
  bool handleNav(GuiNavCommand command) override;

  /// Choose option @p index and tell `on_change` if that changed it.
  void choose(int index);

  /// The option at (@p x, @p y), or -1.
  [[nodiscard]] int optionAt(float x, float y) const;

  /// The choices, top to bottom.
  std::vector<std::string> options{};
  /// The chosen one's index; -1 for none yet.
  int selected = -1;
  /// Called with the new choice's index.
  std::function<void(int)> on_change{};
  /// Height of each option's row.
  float row_height = 28.0f;

private:
  /// The option under the pointer, or -1.
  int hovered_option_ = -1;
};

}  // namespace eng
