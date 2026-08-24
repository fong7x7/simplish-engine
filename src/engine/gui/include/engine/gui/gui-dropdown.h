#pragma once

#include "gui-dropdown-item.h"
#include "gui-dropdown-style.h"
#include "gui-panel.h"

#include <cstdint>
#include <vector>

namespace eng {

/// A dropdown menu that displays a list of selectable items.
/// Extends GuiPanel for background fill, rounded corners, and borders.
/// Typically positioned below a menu bar header or button.
/// Shows only when `visible` is true. Hit testing works on individual
/// items; selecting an item fires its on_select callback.
/// @thread_safety Main thread only.
class GuiDropdown : public GuiPanel {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render the dropdown panel and all items.
  void render(const GuiDrawContext& ctx) const override;

  /// Hit-test which item index is under (mx, my).
  /// Returns -1 if no item is hit.
  int hitTestItem(float mx, float my) const;

  /// Select the item at the given index (fires on_select).
  /// Does nothing if index is out of range.
  void selectItem(int index);

  /// The dropdown items.
  std::vector<GuiDropdownItem> items{};
  /// Visual styling.
  GuiDropdownStyle style{};
  /// Index of the currently hovered item (-1 = none).
  int hovered_item = -1;

private:
  /// Resolved style values for dropdown rendering.
  struct ResolvedStyle {
    /// Background fill color.
    GuiColor bg{};
    /// Text color.
    GuiColor text{};
    /// Hover highlight color.
    GuiColor hover{};
    /// Menu width in pixels.
    int width{0};
    /// Item height in pixels.
    int item_height{0};
  };

  /// Resolve style from shared or per-instance values.
  ResolvedStyle resolveStyle() const;

  /// Render the background panel.
  void renderBackground(const GuiDrawContext& ctx,
                        const ResolvedStyle& rs) const;

  /// Render the hover highlight rect for the row whose top is at @p iy.
  /// Split out of renderItems to keep that loop under the 16-line limit.
  void renderHoverHighlight(const GuiDrawContext& ctx, const ResolvedStyle& rs,
                            float iy) const;

  /// Render all item rows (hover highlight + text).
  void renderItems(const GuiDrawContext& ctx, const ResolvedStyle& rs) const;
};

}  // namespace eng
