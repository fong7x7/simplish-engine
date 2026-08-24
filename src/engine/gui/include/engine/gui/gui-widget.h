#pragma once

/// @file gui-widget.h
/// @brief Abstract base for interactive GUI widgets (tree nodes and overlays).
/// Tree identity: `gui-widget-id.h`; widget kind: `gui-widget-type.h`.

#include "gui-animation.h"
#include "gui-draw-context.h"
#include "gui-mouse-event.h"
#include "gui-rect.h"
#include "gui-scroll-event.h"
#include "gui-style.h"
#include "gui-widget-animator.h"
#include "gui-widget-id.h"
#include "gui-widget-type.h"
#include "layout-engine.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace eng {

// Forward declaration required to break circular dependency:
// gui-widget-tree.h depends on gui-widget.h, and GuiWidget::arrangeChildren
// needs GuiWidgetTree&. Same pattern as markdown-renderer.h.
class GuiWidgetTree;  // NOLINT(no-forward-decl) -- breaks widget/tree cycle

/// Abstract base class for all interactive UI components.
/// Subclasses override render() to draw themselves.
/// Register instances on `GuiWidgetTree` for hit testing, hover, updates,
/// and rendering.
/// @thread_safety Main thread only.
class GuiWidget {
public:
  virtual ~GuiWidget() = default;

  GuiWidget() = default;
  GuiWidget(const GuiWidget&) = default;
  GuiWidget& operator=(const GuiWidget&) = default;
  GuiWidget(GuiWidget&&) = default;
  GuiWidget& operator=(GuiWidget&&) = default;

  /// Polymorphic deep-copy. Returns a new widget with all fields copied
  /// (including handlers). Tree identity fields (widget_id, parent_id,
  /// children) are copied as-is — caller must remap them.
  virtual std::unique_ptr<GuiWidget> clone() const = 0;

  /// Clear transient interaction state (hovered, pressed, tree_state)
  /// and mark dirty for re-layout. Used after adoption into a new tree.
  void resetTransientState();

  /// Per-frame mutable tick (seconds). Default ticks animations.
  virtual void update(const GuiDrawContext& ctx, float dt);

  /// Render this component using the given draw context.
  /// Does not mutate component state; output goes through `ctx`.
  virtual void render(const GuiDrawContext& ctx) const = 0;

  /// Arrange this widget's direct children given its own rect. Called by
  /// `GuiWidgetTree::arrangeWidget` after this widget's rect is set.
  /// Default implementation distributes children uniformly in a column
  /// (preserves the pre-existing tree arrange behaviour). Override to
  /// implement custom layout strategies — e.g. `GuiDockspaceWidget` reserves
  /// edge regions and assigns each child its computed region rect.
  virtual void arrangeChildren(GuiWidgetTree& tree, const Rect& available);

  /// Subscribes to mouse button being pressed on this component.
  void onMouseDown(const std::function<void(const GuiMouseEvent&)>& handler);

  /// Subscribes to mouse button being released on this component.
  void onMouseUp(const std::function<void(const GuiMouseEvent&)>& handler);

  /// Subscribes to mouse move events over this component.
  void onMouseMove(const std::function<void(const GuiMouseEvent&)>& handler);

  /// Subscribes to scroll events over this component.
  void onScroll(const std::function<void(const GuiScrollEvent&)>& handler);

  /// Subscribes to click events on this component.
  void onClick(const std::function<void(const GuiMouseEvent&)>& handler);

  /// Removes all registered click handlers.
  void clearClickHandlers();

  /// True if at least one click handler is registered.
  [[nodiscard]] bool hasClickHandlers() const;

  /// Called when a mouse button is pressed inside this component.
  /// Return true to capture subsequent move/up events until release.
  virtual bool handleMouseDown(const GuiMouseEvent& event);

  /// Called when a mouse button is released (only if this component captured).
  virtual void handleMouseUp(const GuiMouseEvent& event);

  /// Called on mouse motion while this component has capture.
  virtual void handleMouseMove(const GuiMouseEvent& event);

  /// Called when the scroll wheel moves over this component.
  /// Return true if the event was consumed.
  virtual bool handleScroll(const GuiScrollEvent& event);

  /// Called when the mouse clicks on this component.
  /// Return true if the event was consumed.
  virtual bool handleClick(const GuiMouseEvent& event);

  /// Test whether point (mx, my) is inside this component's rect.
  bool isInside(float mx, float my) const;

  /// True when the component should read colors from `ui_style` instead
  bool hasSharedStyle() const;

  /// Start a property animation. Replaces any existing animation on the
  /// same property.
  void animate(const GuiAnimation& anim);

  /// Animate opacity from 0 to 1 over the given duration.
  void fadeIn(float duration, GuiEasing easing);

  /// Animate opacity from current to 0 over the given duration.
  void fadeOut(float duration, GuiEasing easing);

  /// Animate rect to the target position/size over the given duration.
  void slideTo(const Rect& target, float duration, GuiEasing easing);

  /// Cancel all active animations on this widget.
  void cancelAnimations();

  /// True if any animations are currently running.
  bool isAnimating() const;

  /// Unique string identifier for lookup and end-to-end testing.
  /// Empty string means no ID assigned.
  std::string id{};
  /// Bounds in logical pixels (manual placement or tree layout output).
  Rect rect{};
  /// Whether the mouse cursor is over this component.
  bool hovered = false;
  /// Whether the mouse button is held down on this component.
  bool pressed = false;
  /// Whether this component is rendered and hit-testable.
  bool visible = true;
  /// Opacity multiplier [0, 1] applied to all colors during rendering.
  float opacity = 1.0f;
  /// Pointer to the active shared style (not owned).
  /// When non-null and override_style is false, components read colors
  /// from this style. When null, components use their inline fields.
  const GuiStyle* ui_style = nullptr;
  /// When true, ignore the shared style and use per-instance fields.
  bool override_style = false;

  /// Numeric id when this component is a node in `GuiWidgetTree`.
  GuiWidgetId widget_id = GUI_WIDGET_ID_INVALID;
  /// Parent node id in the retained tree (`GUI_WIDGET_ID_INVALID` if root).
  GuiWidgetId parent_id = GUI_WIDGET_ID_INVALID;
  /// Child node ids in tree iteration order.
  std::vector<GuiWidgetId> children{};
  /// Kind for theming and tooling (panel, button, etc.).
  GuiWidgetType widget_type = GuiWidgetType::CUSTOM;
  /// Flex and sizing when participating in tree layout.
  LayoutStyle tree_layout{};
  /// Paint and hit-test order among siblings; higher draws later (on top).
  int32_t z_index = 0;
  /// Optional debug label for editors and tests.
  std::string debug_name{};
  /// Whether this tree node participates in keyboard focus order.
  bool tree_focusable = false;
  /// Subtree needs measure/arrange.
  bool tree_dirty = true;
  /// Node needs redraw.
  bool tree_render_dirty = true;
  /// Whether this widget is registered as an overlay component.
  /// Set by GuiWidgetTree::registerComponent. When true, visitDrawOrder
  /// skips this node (it renders via renderSortedOverlays instead).
  bool overlay_registered = false;

private:
  /// Click callbacks registered for this widget (routed after hit-test).
  std::vector<std::function<void(const GuiMouseEvent&)>> on_click_handlers_{};
  /// Mouse-button-down callbacks for this widget.
  std::vector<std::function<void(const GuiMouseEvent&)>>
      on_mouse_down_handlers_{};
  /// Mouse-button-up callbacks for this widget.
  std::vector<std::function<void(const GuiMouseEvent&)>>
      on_mouse_up_handlers_{};
  /// Mouse-move callbacks for this widget.
  std::vector<std::function<void(const GuiMouseEvent&)>>
      on_mouse_move_handlers_{};
  /// Scroll-wheel callbacks for this widget.
  std::vector<std::function<void(const GuiScrollEvent&)>> on_scroll_handlers_{};
  /// Per-widget animation state (active animations ticked in update).
  GuiWidgetAnimator animator_{};
};
}  // namespace eng
