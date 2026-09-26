#pragma once

/// @file gui-tabs.h
/// @brief A row of tabs, one selected, with a sliding underline.
/// @par Threading
/// Main thread only.

#include "gui-widget.h"

#include <functional>
#include <string>
#include <vector>

namespace eng {

/// A tab bar: labels in a row over a hairline, the selected one in the
/// text colour with the accent underline, which slides to a new tab. A
/// click selects; LEFT and RIGHT move while it has focus. Show and hide
/// each tab's page in `on_change`.
/// @thread_safety Main thread only.
class GuiTabs : public GuiWidget {
public:
  /// An empty, focusable tab bar.
  GuiTabs();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Slide the underline towards the selected tab.
  void update(const GuiDrawContext& ctx, float dt) override;

  /// The tabs, the hairline, the underline.
  void render(const GuiDrawContext& ctx) const override;

  /// Every tab side by side, one line and the underline high.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// Select the tab under the pointer.
  bool handleClick(const GuiMouseEvent& event) override;

  /// Light the tab under the pointer.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// LEFT and RIGHT select the neighbours.
  bool handleNav(GuiNavCommand command) override;

  /// Select tab @p index and tell `on_change` if that changed it.
  void select(int index);

  /// The tab at (@p x, @p y), or -1; needs a draw context to measure.
  [[nodiscard]] int tabAt(const GuiDrawContext& ctx, float x, float y) const;

  /// Tab labels, left to right.
  std::vector<std::string> tabs{};
  /// The selected tab.
  int selected = 0;
  /// Called with the new tab's index.
  std::function<void(int)> on_change{};

private:
  /// Draw the hairline and the accent underline under the tabs at @p spans.
  void drawUnderline(const GuiDrawContext& ctx,
                     const std::vector<std::pair<float, float>>& spans) const;

  /// Each tab's left edge and width, from the last update.
  std::vector<std::pair<float, float>> spans_{};
  /// Where the underline is drawn, easing towards the selected tab.
  std::pair<float, float> underline_{0.0f, 0.0f};
  /// The tab under the pointer, or -1.
  int hovered_tab_ = -1;
};

}  // namespace eng
