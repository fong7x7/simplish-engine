#pragma once

/// @file gui-widget.h
/// @brief Abstract base for interactive GUI widgets (tree nodes and overlays).
/// Tree identity: `gui-widget-id.h`; widget kind: `gui-widget-type.h`.

#include "gui-animation.h"
#include "gui-draw-context.h"
#include "gui-focus-change.h"
#include "gui-mouse-event.h"
#include "gui-nav-command.h"
#include "gui-rect.h"
#include "gui-scroll-event.h"
#include "gui-state-styles.h"
#include "gui-style-transition.h"
#include "gui-theme.h"
#include "gui-widget-animator.h"
#include "gui-widget-id.h"
#include "gui-widget-state.h"
#include "gui-widget-type.h"
#include "layout-engine.h"
#include "layout-size.h"

#include <functional>
#include <optional>
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

  /// The size this widget's own content needs — its text, say — without
  /// padding or children, given at most @p max_width across (negative
  /// for no limit), which wrapping text wraps to. The measure pass adds
  /// the padding and takes the larger of this and what the children need.
  /// Default: nothing.
  [[nodiscard]] virtual LayoutSize measureContent(const GuiDrawContext& ctx,
                                                  float max_width) const;

  /// Arrange this widget's direct children given its own rect. Called by
  /// `GuiWidgetTree::arrangeWidget` after this widget's rect is set.
  /// The default lays them out by flexbox, from this widget's
  /// `tree_layout` and theirs (`layout-engine.h`). Override to implement
  /// custom layout strategies — e.g. `GuiDockspaceWidget` reserves edge
  /// regions and assigns each child its computed region rect.
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

  /// Subscribes to this widget taking or losing navigation focus.
  void onFocusChange(const std::function<void(GuiFocusChange)>& handler);

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

  /// Called with a navigation command while this widget, or one of its
  /// descendants that did not take it, has focus. Return true if consumed;
  /// an unconsumed command goes on to the parent, and then to the tree's
  /// own focus movement. The default presses the widget on CONFIRM when it
  /// has click handlers, as a click at its centre would.
  virtual bool handleNav(GuiNavCommand command);

  /// Called when this widget takes or loses focus (`GuiWidgetTree`
  /// focus, from navigation, a click, or `setFocus`). The default fires the
  /// `onFocusChange` handlers.
  virtual void handleFocusChange(GuiFocusChange change);

  /// Scroll so @p child, a descendant's rect, shows inside this widget.
  /// Return true if the scroll moved; the tree then calls
  /// `arrangeAfterScroll`. Default: this widget does not scroll.
  virtual bool revealChild(const Rect& child);

  /// Scroll by (@p dx, @p dy) pixels — a pad's right stick, say. Return
  /// true if it moved. Default: this widget does not scroll.
  virtual bool scrollBy(float dx, float dy);

  /// Scroll one step in @p command's direction, for a pad or arrow key
  /// that found nothing focusable that way. Return true if it moved.
  /// Default: this widget does not scroll.
  virtual bool scrollByNav(GuiNavCommand command);

  /// Re-lay out whatever a scroll moved, after `handleScroll`,
  /// `revealChild` or `scrollByNav` reported one. Default: nothing.
  virtual void arrangeAfterScroll(GuiWidgetTree& tree);

  /// The rect this widget's children are drawn clipped to, or nothing for
  /// no clipping. Default: nothing.
  [[nodiscard]] virtual std::optional<Rect> childClipRect() const;

  /// Test whether point (mx, my) is inside this component's rect.
  bool isInside(float mx, float my) const;

  /// The state this widget is drawn in, from `disabled`, `pressed`,
  /// `selected`, `hovered` and `focused`, in that order of precedence.
  [[nodiscard]] virtual GuiWidgetState visualState() const;

  /// The look this kind of widget takes from @p theme in each state — a
  /// button's variant, a field's — or null for a widget the theme does not
  /// style. Default: null.
  [[nodiscard]] virtual const GuiStateStyles*
  themeStyles(const GuiTheme& theme) const;

  /// The styles this widget draws with: its own `state_styles` when set,
  /// else `themeStyles`. Null when it has neither.
  [[nodiscard]] const GuiStateStyles* activeStyles(const GuiTheme& theme) const;

  /// What to draw now: the blend `update` is running between two states'
  /// looks, else the look for `visualState()`. Only meaningful when
  /// `activeStyles` is not null.
  [[nodiscard]] GuiStateStyle drawnStyle(const GuiDrawContext& ctx) const;

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
  /// Whether the pointer passes through this widget to whatever is under
  /// it: it is never the widget hit itself, though its children may be.
  /// A see-through overlay — a game's HUD over its view — sets it.
  bool pointer_through = false;
  /// Opacity multiplier [0, 1] applied to all colors during rendering —
  /// the widget's own, and, in a tree, its whole subtree's.
  float opacity = 1.0f;
  /// Drawn scaled by this about its centre, with its subtree: a button
  /// pressed in, a dialog popping up. Layout and hit testing ignore it,
  /// as they ignore a CSS transform's effect on flow.
  float render_scale = 1.0f;
  /// Drawn moved right by this many layout pixels, with its subtree.
  float render_offset_x = 0.0f;
  /// Drawn moved down by this many layout pixels, with its subtree.
  float render_offset_y = 0.0f;
  /// Turned off: drawn in the DISABLED style, clicks and navigation pass
  /// it by, and focus skips it.
  bool disabled = false;
  /// Chosen — the active tool, the open tab — and drawn so.
  bool selected = false;
  /// Holding keyboard or pad focus; kept by `handleFocusChange`.
  bool focused = false;
  /// This widget's own look in every state, in place of the theme's.
  std::optional<GuiStateStyles> state_styles{};

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
  /// Border-box size from the last measure pass: the explicit size, or the
  /// content's plus padding, within the min and max. Margins not included.
  LayoutSize tree_measured{};
  /// The width limit that measurement used, so `updateLayout` knows when
  /// it still holds.
  float tree_measured_limit = -2.0f;
  /// Paint and hit-test order among siblings; higher draws later (on top).
  int32_t z_index = 0;
  /// Optional debug label for editors and tests.
  std::string debug_name{};
  /// Shown in a small box by the tree after the pointer rests on the
  /// widget for `GUI_TOOLTIP_DELAY_SECONDS`; empty for none.
  std::string tooltip{};
  /// Whether this tree node can take focus from keyboard or pad navigation.
  /// Buttons, sliders, dropdowns and text fields set it themselves; a
  /// custom widget sets it to join in.
  bool tree_focusable = false;
  /// Subtree needs measure/arrange.
  bool tree_dirty = true;
  /// Node needs redraw.
  bool tree_render_dirty = true;

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
  /// Focus gained and lost callbacks for this widget.
  std::vector<std::function<void(GuiFocusChange)>> on_focus_handlers_{};
  /// Per-widget animation state (active animations ticked in update).
  GuiWidgetAnimator animator_{};
  /// Blends the drawn style between states; ticked in update.
  GuiStyleTransition style_transition_{};
};
}  // namespace eng
