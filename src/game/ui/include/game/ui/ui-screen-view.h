#pragma once

/// @file ui-screen-view.h
/// @brief A game screen built out of the engine's GUI widgets.
/// @par Threading
/// Main-thread-only, with the widget tree it builds into.

#include <engine/gui/gui-widget-id.h>
#include <engine/gui/gui-widget-tree.h>
#include <functional>
#include <game/ui/ui-button-info.h>
#include <game/ui/ui-screen.h>
#include <game/ui/ui-values.h>
#include <string>
#include <string_view>
#include <vector>

namespace eng::game {

/// One screen, built: a `GuiPanel` covering its parent — dimmed, for a
/// menu — holding the screen's root placed by its anchor, and under it a
/// `GuiPanel`, `GuiLabel` or `GuiButton` for every node, styled by the
/// flexbox the file gives. The view owns every string its widgets show,
/// fills `{key}`s from the values it is given, and reports a button
/// pressed — a click, or the pad's confirm on the focused one — by its
/// action.
///
/// Its widgets call back into it, so it neither copies nor moves: hold it
/// by pointer. Layout is the tree's: `apply` says when to run it again.
class UiScreenView {
public:
  /// What hears a button pressed: its action.
  using OnAction = std::function<void(std::string_view action)>;

  /// A view of @p screen, reporting presses to @p on_action.
  UiScreenView(UiScreen screen, OnAction on_action);

  /// Build the widgets under @p parent in @p tree, showing no values yet;
  /// the covering panel's id.
  GuiWidgetId build(GuiWidgetTree& tree, GuiWidgetId parent);

  /// Show @p values: fill every text, size every bar. Whether anything
  /// changed, so that the tree's layout must be run again.
  bool apply(GuiWidgetTree& tree, const UiValues& values);

  /// Take the widgets out of @p tree.
  void destroy(GuiWidgetTree& tree);

  /// The panel covering the parent; invalid before `build`.
  [[nodiscard]] GuiWidgetId overlay() const { return overlay_; }

  /// The screen it shows.
  [[nodiscard]] const UiScreen& screen() const { return screen_; }

  /// The first button, to give focus to; invalid when there is none.
  [[nodiscard]] GuiWidgetId firstButton() const;

  /// Every button, in the order the file gives them, where @p tree last
  /// laid them out.
  [[nodiscard]] std::vector<UiButtonInfo>
  buttons(const GuiWidgetTree& tree) const;

  UiScreenView(const UiScreenView&) = delete;
  UiScreenView& operator=(const UiScreenView&) = delete;
  UiScreenView(UiScreenView&&) = delete;
  UiScreenView& operator=(UiScreenView&&) = delete;
  ~UiScreenView() = default;

private:
  /// A label or a button's text: the pattern, and what it shows now.
  struct TextSlot {
    /// The widget showing it.
    GuiWidgetId widget = GUI_WIDGET_ID_INVALID;
    /// The text as written, `{key}`s and all.
    std::string pattern{};
    /// What it shows now, which the widget's view points into.
    std::string shown{};
  };

  /// A bar: its two parts, and the keys that size them.
  struct BarSlot {
    /// The filled part.
    GuiWidgetId fill = GUI_WIDGET_ID_INVALID;
    /// The empty part.
    GuiWidgetId rest = GUI_WIDGET_ID_INVALID;
    /// The value's key.
    std::string value{};
    /// The full value's key, or a number.
    std::string max{};
    /// The share it shows now, 0 to 1.
    float shown = -1.0F;
  };

  /// A button: its widget and what it chooses.
  struct ButtonSlot {
    /// The widget.
    GuiWidgetId widget = GUI_WIDGET_ID_INVALID;
    /// Its node's id.
    std::string id{};
    /// Its action.
    std::string action{};
    /// Its text slot's index in `texts_`.
    size_t text = 0;
  };

  /// Build @p node and its children under @p parent.
  void buildNode(GuiWidgetTree& tree, GuiWidgetId parent, const UiNode& node);
  /// Build the widget for @p node — not its children — under @p parent;
  /// its id.
  GuiWidgetId buildOne(GuiWidgetTree& tree, GuiWidgetId parent,
                       const UiNode& node);
  /// Build a label for @p node under @p parent; its widget.
  GuiWidgetId buildLabel(GuiWidgetTree& tree, GuiWidgetId parent,
                         const UiNode& node);
  /// Build a button for @p node under @p parent; its widget.
  GuiWidgetId buildButton(GuiWidgetTree& tree, GuiWidgetId parent,
                          const UiNode& node);
  /// Build a bar for @p node under @p parent; its track.
  GuiWidgetId buildBar(GuiWidgetTree& tree, GuiWidgetId parent,
                       const UiNode& node);
  /// Fill the text of @p slot from @p values into its widget; whether it
  /// changed.
  static bool fillText(GuiWidgetTree& tree, TextSlot& slot,
                       const UiValues& values);
  /// Size @p slot's parts from @p values; whether it changed.
  static bool fillBar(GuiWidgetTree& tree, BarSlot& slot,
                      const UiValues& values);

  /// The screen shown.
  UiScreen screen_;
  /// What hears a press.
  OnAction on_action_;
  /// The covering panel.
  GuiWidgetId overlay_ = GUI_WIDGET_ID_INVALID;
  /// Every text shown, in build order.
  std::vector<TextSlot> texts_{};
  /// Every bar, in build order.
  std::vector<BarSlot> bars_{};
  /// Every button, in build order.
  std::vector<ButtonSlot> buttons_{};
};

}  // namespace eng::game
