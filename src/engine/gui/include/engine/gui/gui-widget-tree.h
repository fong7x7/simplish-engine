#pragma once

/// @file gui-widget-tree.h
/// @brief The retained widget tree: ownership, layout, hit testing,
/// focus, rendering, and the overlay layer popovers and dialogs live in.
/// @par Threading Main thread only.

#include "adopt-result.h"
#include "gui-draw-context.h"
#include "gui-focus-visibility.h"
#include "gui-input.h"
#include "gui-nav-command.h"
#include "gui-text-input.h"
#include "gui-widget.h"
#include "layout-engine.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace eng {

using GuiWidgetVisitor = std::function<void(const GuiWidget&)>;
using GuiWidgetMutVisitor = std::function<void(GuiWidget&)>;

/// Seconds the pointer rests on a widget before its tooltip shows.
inline constexpr float GUI_TOOLTIP_DELAY_SECONDS = 0.5f;

/// `z_index` of the overlay layer: above anything else under the root.
inline constexpr int32_t GUI_OVERLAY_LAYER_Z = 1000000;

/// @thread_safety Main thread only.
class GuiWidgetTree {
public:
  /// Tree-owned component nodes (`unique_ptr`); key is `widget_id`.
  std::unordered_map<GuiWidgetId, std::unique_ptr<GuiWidget>> widget_nodes{};
  /// ID of the root widget in the tree.
  GuiWidgetId root_id = GUI_WIDGET_ID_INVALID;
  /// Next ID to assign for new widgets (monotonically increasing).
  GuiWidgetId next_id = 1;
  /// ID of the widget with keyboard focus (GUI_WIDGET_ID_INVALID if none).
  GuiWidgetId focused_id = GUI_WIDGET_ID_INVALID;
  /// ID of the widget under the mouse cursor (GUI_WIDGET_ID_INVALID if none).
  GuiWidgetId hovered_id = GUI_WIDGET_ID_INVALID;
  /// ID of the widget being mouse-pressed (GUI_WIDGET_ID_INVALID if none).
  GuiWidgetId pressed_id = GUI_WIDGET_ID_INVALID;
  /// The subtree navigation stays inside — an open menu or dialog — or
  /// GUI_WIDGET_ID_INVALID for the whole tree. See `setFocusScope`.
  GuiWidgetId focus_scope_id = GUI_WIDGET_ID_INVALID;
  /// Whether the focus ring is drawn: shown by navigation, hidden by the
  /// pointer.
  GuiFocusVisibility focus_visibility = GuiFocusVisibility::HIDDEN;
  /// Round arranged edges to multiples of this many layout pixels — one
  /// device pixel, `1 / density`, keeps edges crisp; 0 leaves them where
  /// the layout put them. The rendered client sets it.
  float pixel_snap = 0.0f;

  /// Allocate a widget and attach to parent. Returns GUI_WIDGET_ID_INVALID on
  /// failure.
  GuiWidgetId createWidget(GuiWidgetType type, GuiWidgetId parent);

  /// Insert an externally-created widget into the tree. Assigns a fresh ID,
  /// sets parent, and attaches to parent's children. Ownership transfers to
  /// the tree. Returns the assigned widget ID.
  GuiWidgetId insertExternalWidget(std::unique_ptr<GuiWidget> widget,
                                   GuiWidgetId parent);

  /// Remove a widget and all its descendants from the tree.
  void destroyWidget(GuiWidgetId id);

  /// Lookup a tree node by id. Returns nullptr if not found.
  GuiWidget* findWidget(GuiWidgetId id);
  const GuiWidget* findWidget(GuiWidgetId id) const;

  /// Move a subtree to a new parent. Returns false if it would create a cycle.
  bool reparentWidget(GuiWidgetId id, GuiWidgetId new_parent);

  /// Flag a widget and its ancestors for re-layout.
  void markDirty(GuiWidgetId id);

  /// Reset all dirty flags after the layout pass.
  void clearDirtyFlags();

  /// Pre-order traversal (parent before children) for layout arrange pass.
  void visitPreOrder(GuiWidgetId root, const GuiWidgetVisitor& visitor) const;

  /// Post-order traversal (children before parent) for layout measure pass.
  void visitPostOrder(GuiWidgetId root, const GuiWidgetVisitor& visitor) const;

  /// Mutable pre-order traversal.
  void visitPreOrder(GuiWidgetId root, const GuiWidgetMutVisitor& visitor);

  /// Pre-order visit from `root_id`: parent then children sorted by `z_index`
  /// ascending (back-to-front paint order among siblings).
  void visitDrawOrder(const GuiWidgetVisitor& visitor) const;

  /// Deep-copy a subtree from `source` into this tree under `target_parent`.
  /// Clones all widgets (including handlers), assigns fresh IDs, and remaps
  /// parent/child relationships. Returns the new root ID of the adopted
  /// subtree, or GUI_WIDGET_ID_INVALID on failure.
  GuiWidgetId adoptSubtree(const GuiWidgetTree& source, GuiWidgetId source_root,
                           GuiWidgetId target_parent);

  /// Same as adoptSubtree but also returns a mapping from source IDs to
  /// newly assigned target IDs (for callers that store individual widget IDs).
  AdoptResult adoptSubtreeWithMap(const GuiWidgetTree& source,
                                  GuiWidgetId source_root,
                                  GuiWidgetId target_parent);

  /// Return the number of direct children.
  size_t childCount(GuiWidgetId id) const;

  /// The layer overlays live in — popovers, menus, modals, toasts: a
  /// see-through panel over the whole root, drawn and hit above
  /// everything else under it, made on first call. Its children are placed
  /// by hand (`PositionMode::MANUAL`, with `placePopover`) or by insets
  /// (`ABSOLUTE`). Invalid until the tree has a root.
  GuiWidgetId overlayLayer();

  /// Whether the tree changed since `computeLayout` last ran — a widget
  /// added, removed or `markDirty`-ed — so the host should lay it out.
  [[nodiscard]] bool needsLayout() const;

  /// Lay the tree out in @p viewport, the screen rect: measure every
  /// widget bottom-up, then arrange from the root, which takes the whole
  /// viewport. Text is measured with @p ctx's font.
  void computeLayout(const Rect& viewport, const GuiDrawContext& ctx);

  /// `computeLayout` with no font: text measures at a fixed width a
  /// character, as it does when drawn without one.
  void computeLayout(const Rect& viewport);

  /// Lay out again only what changed: subtrees that are clean
  /// (`markDirty` not called on anything in them) and keep their box are
  /// left as they are. As `computeLayout` otherwise — call `markDirty` on a
  /// widget whose content changed, a label's text say, for it to count.
  void updateLayout(const Rect& viewport, const GuiDrawContext& ctx);

  /// Measure a single subtree bottom-up (post-order), setting each
  /// widget's `tree_measured`, with no limit on its width.
  void measureWidget(GuiWidgetId id, const GuiDrawContext& ctx);

  /// As above, the subtree at most @p max_width wide (negative for no
  /// limit): what wrapping text inside it wraps to.
  void measureWidget(GuiWidgetId id, const GuiDrawContext& ctx,
                     float max_width);

  /// Arrange a single subtree top-down within the available rect.
  void arrangeWidget(GuiWidgetId id, const Rect& available);

  /// Find the topmost visible widget under a screen coordinate.
  HitTestResult hitTest(float x, float y) const;

  /// Process a mouse event. Returns true if a widget was hit.
  bool routeMouseEvent(const GuiMouseEvent& event);

  /// Process a keyboard event. Returns true if consumed.
  bool routeKeyEvent(const GuiKeyEvent& event);

  /// Process composed text input (from IME or direct typing).
  bool routeTextInput(std::string_view text);

  /// Set keyboard focus to a specific widget. No-op if not focusable.
  void setFocus(GuiWidgetId id);

  /// Focus @p widget, a node of this tree. No-op if it is not one, or not
  /// focusable.
  void setFocus(GuiWidget& widget);

  /// The widget with focus, or null.
  [[nodiscard]] GuiWidget* focusedWidget();
  [[nodiscard]] const GuiWidget* focusedWidget() const;

  /// Cycle focus to the next or previous focusable widget in the focus
  /// scope, in tree order, wrapping.
  void advanceFocus(FocusTraversalDirection direction);

  /// Move focus to the nearest focusable widget in the scope that lies in
  /// @p command's direction — UP, DOWN, LEFT or RIGHT — scored by distance
  /// with a penalty for being off the line, so a grid moves by rows and
  /// columns. Returns false, leaving focus where it is, when nothing lies
  /// that way or @p command is not a direction.
  bool navigateFocus(GuiNavCommand command);

  /// Carry out one navigation command — from a pad, a keyboard, anything.
  ///
  /// With nothing focused, any command but CANCEL focuses the scope's first
  /// focusable widget. Otherwise the focused widget and then each ancestor
  /// up to the scope's root is offered it (`GuiWidget::handleNav`): a
  /// button presses on CONFIRM, a slider steps on LEFT and RIGHT, an open
  /// dropdown moves through its rows. CONFIRM on a text field starts typing
  /// in it and CANCEL stops. What nothing takes becomes focus movement:
  /// directions spatially, NEXT and PREVIOUS in order. Shows the focus ring.
  ///
  /// Returns false for a command nothing used — a CANCEL no widget took is
  /// the caller's, to close the menu.
  bool routeNav(GuiNavCommand command);

  /// Scroll the nearest ancestor of the focused widget that scrolls by
  /// (@p dx, @p dy) pixels — what a pad's right stick does. True if one
  /// moved.
  bool scrollFocusBy(float dx, float dy);

  /// Keep navigation inside @p scope's subtree — the menu or dialog that
  /// is open — and move focus into it when it is outside.
  /// GUI_WIDGET_ID_INVALID lets it range over the whole tree again.
  void setFocusScope(GuiWidgetId scope);


  void updateHover(float mx, float my);

  /// Per-frame tick for every tree widget.
  void updateAll(const GuiDrawContext& ctx, float dt);

  bool dispatchClick(float mx, float my);

  void renderAll(const GuiDrawContext& ctx);

  bool anyHovered() const;

  GuiWidget* findById(std::string_view component_id);

  const GuiWidget* findById(std::string_view component_id) const;

  bool dispatchMouseDown(const GuiMouseEvent& event);

  void dispatchMouseUp(const GuiMouseEvent& event);

  void dispatchMouseMove(const GuiMouseEvent& event);

  bool dispatchScroll(const GuiScrollEvent& event);

  bool hasCapture() const;

  bool dispatchText(std::string_view text);

  bool dispatchKey(uint32_t keycode);

  bool dispatchKey(const GuiKeyEvent& event);

  bool hasFocusedInput() const;

  void clearFocus();


private:
  /// Flags indicating which cursor-relevant widget types are hovered.
  struct HoveredCursorFlags {
    /// A TEXT_INPUT widget is hovered.
    bool text_input = false;
    /// A BUTTON or widget with click handlers is hovered.
    bool clickable = false;
  };

  /// Update flags for a single widget's cursor-relevant hover state.
  static void accumulateCursorFlags(HoveredCursorFlags& flags,
                                    const GuiWidget& w);

  /// Scan all widgets for hovered cursor-relevant types.
  [[nodiscard]] HoveredCursorFlags scanHoveredCursorFlags() const;

  /// Determine the cursor shape from the currently hovered widget type.
  [[nodiscard]] GuiCursorShape cursorShapeForHover() const;

  /// Handle arrow/home/end key for focused text input.
  static bool handleTextNavKey(GuiTextInput& input, const GuiKeyEvent& event);

  /// Handle Enter key — inserts newline for TEXT_AREA widgets only.
  static bool handleReturnKey(GuiTextInput& input);

  /// Handle backspace or delete key for focused text input.
  static bool handleDeletionKey(GuiTextInput& input, uint32_t keycode);

  /// Handle backspace/delete/select-all/clipboard keys for focused input.
  static bool handleTextEditKey(GuiTextInput& input, const GuiKeyEvent& event);

  /// Handle Ctrl/Cmd+C/X/V clipboard shortcuts.
  static bool handleClipboardKey(GuiTextInput& input, const GuiKeyEvent& event);

  /// Dispatch a single clipboard action by keycode (C, X, or V).
  static bool dispatchClipboardAction(GuiTextInput& input, uint32_t keycode);

  /// Copy selected text to system clipboard then delete the selection.
  static void cutToClipboard(GuiTextInput& input);

  /// Copy selected text to system clipboard.
  static void copyToClipboard(const GuiTextInput& input);

  /// Paste system clipboard text into input at cursor.
  static void pasteFromClipboard(GuiTextInput& input);

  /// Every visible focusable widget navigation can reach: the focus
  /// scope's, in tree order.
  [[nodiscard]] std::vector<GuiWidget*> focusableInScope();

  /// The root navigation searches from: the focus scope, or the tree root.
  [[nodiscard]] GuiWidgetId navRoot() const;

  /// Whether @p widget is a node of this tree.
  [[nodiscard]] bool isTreeNode(const GuiWidget& widget) const;

  /// Whether the focused widget is one navigation can reach.
  [[nodiscard]] bool hasNavFocus();

  /// Offer @p command to the focused widget and, for a tree widget, its
  /// ancestors up to the scope's root; true if one took it.
  bool bubbleNav(GuiNavCommand command);

  /// CONFIRM and CANCEL as they apply to the text field that is focused
  /// or typing, if any; true if they were used.
  bool routeTextNav(GuiNavCommand command);

  /// With nothing focused: focus the first widget navigation can reach, for
  /// any @p command but CANCEL. True if it did.
  bool focusFirst(GuiNavCommand command);

  /// @p command as focus movement: NEXT and PREVIOUS in order, directions
  /// spatially. True if focus moved, or the command was NEXT or PREVIOUS.
  bool moveNavFocus(GuiNavCommand command);

  /// A direction nothing lies in, as a scroll of the focused widget's
  /// nearest scrolling ancestor; true if one moved.
  bool scrollNav(GuiNavCommand command);

  /// Focus @p widget — a tree node, or null for nothing —
  /// dropping typing focus from any other text field, and scrolling it
  /// into view.
  void moveFocus(GuiWidget* widget);

  /// Tell @p was it lost focus and @p now it gained it; either may be null.
  static void announceFocusChange(GuiWidget* was, GuiWidget* now);

  /// Scroll every scrolling ancestor of the focused tree widget so it
  /// shows, innermost first.
  void revealFocus();

  /// Let @p widget re-lay itself out after it scrolled.
  void afterScroll(GuiWidget& widget);

  /// The part of the screen @p widget's ancestors let it draw in, or
  /// nothing when none of them clips.
  [[nodiscard]] std::optional<Rect> ancestorClip(const GuiWidget& widget) const;

  /// Ring the focused widget, when the ring is shown, clipped as the widget
  /// itself is.
  void renderFocusRing(const GuiDrawContext& ctx) const;

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Draw @p id and its children in paint order, clipping the children to
  /// whatever `childClipRect` it asks for.
  void renderTreeNode(GuiWidgetId id, const GuiDrawContext& ctx) const;


  /// Update the system cursor shape based on hovered widget type.
  void updateCursorForHover();

  /// Handle click with double-click detection and text input focus.
  void handleClickWithDoubleCheck(const GuiMouseEvent& event, GuiWidget& target,
                                  GuiSelectionExtend drag_mode);

  /// Process a confirmed mouse-down hit on a widget (capture or defer click).
  bool handleMouseDownHit(GuiWidget& comp, const GuiMouseEvent& event);

  /// Begin drag-selection if the mouseDown target is a text input.
  void beginTextInputDrag(GuiWidget* hit, float mx);

  /// Check timing and proximity for a double-click.
  [[nodiscard]] bool checkDoubleClick(float mx, float my) const;

  /// Record click time and position for future double-click checks.
  void recordClickTiming(float mx, float my);

  /// Clear hovered flag on all tree widgets.
  void clearTreeHover();

  /// Set hovered flag on the given widget and all its ancestors.
  void applyTreeHover(GuiWidgetId leaf_id);

  /// Dispatch handleMouseMove to the topmost hovered tree node.
  void routeHoverMove(float mx, float my);

  void updateTextInputFocus(float mx, float my, GuiSelectionExtend sel_mode);

  /// Swap focused text input, unfocusing the old and focusing the new.
  void applyTextInputFocus(GuiTextInput* hit);


  /// If the last mouse-down did not capture, fire `onClick` on matching
  /// button-up inside the same widget (desktop press/release path).
  void tryFireClickFromMouseUp(const GuiMouseEvent& event,
                               GuiSelectionExtend drag_mode);

  /// Clone a single widget from source into target, assigning a fresh ID.
  /// Does not recurse into children.
  static GuiWidgetId cloneNodeInto(GuiWidgetTree& target, const GuiWidget& src,
                                   GuiWidgetId target_parent);

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Recursive helper for adoptSubtree: clones one node and its children.
  static GuiWidgetId adoptSubtreeRec(GuiWidgetTree& target,
                                     const GuiWidgetTree& source,
                                     GuiWidgetId source_id,
                                     GuiWidgetId target_parent);

  /// Recursive helper that also populates an old→new ID map.
  struct AdoptMapContext {
    /// Target tree receiving the adopted widgets.
    GuiWidgetTree& target;
    /// Source tree being copied from.
    const GuiWidgetTree& source;
    /// Mapping from source IDs to newly assigned target IDs.
    std::unordered_map<GuiWidgetId, GuiWidgetId>& id_map;
  };

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Recursive helper for adoptSubtreeWithMap.
  static GuiWidgetId adoptSubtreeMapRec(const AdoptMapContext& ctx,
                                        GuiWidgetId source_id,
                                        GuiWidgetId target_parent);

  /// Create the concrete widget subtype for a given widget type.
  static std::unique_ptr<GuiWidget> makeTreeNode(GuiWidgetType type);

  /// Return true if `ancestor_id` is an ancestor of `id` in the tree.
  static bool isAncestor(const GuiWidgetTree& tree, GuiWidgetId id,
                         GuiWidgetId ancestor_id);

  /// Remove `child_id` from the children list of `parent_id`.
  static void removeFromParent(GuiWidgetTree& tree, GuiWidgetId child_id,
                               GuiWidgetId parent_id);

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Collect all descendant IDs of `id` into `out` (recursive).
  static void collectDescendants(const GuiWidgetTree& tree, GuiWidgetId id,
                                 std::vector<GuiWidgetId>& out);

  /// Clear focused/hovered/pressed references if they match `id`.
  static void clearReferences(GuiWidgetTree& tree, GuiWidgetId id);

  /// Allocate a widget node and insert it into the map (no parent link).
  static GuiWidgetId insertWidget(GuiWidgetTree& tree, GuiWidgetType type,
                                  GuiWidgetId parent);

  /// Link a widget into its parent's children list (or set as root).
  static void attachWidget(GuiWidgetTree& tree, GuiWidgetId id,
                           GuiWidgetId parent);

  /// Unlink a widget from its parent and clear root if necessary.
  static void detachFromTree(GuiWidgetTree& tree, GuiWidgetId id,
                             GuiWidgetId parent_id);

  /// Erase a batch of widget IDs from the tree, clearing references.
  static void eraseWidgets(GuiWidgetTree& tree,
                           const std::vector<GuiWidgetId>& ids);

  /// Return z_index of a widget, or 0 if not found.
  static int32_t nodeZIndex(const GuiWidgetTree& tree, GuiWidgetId id);

  /// Return parent's children sorted by ascending z_index.
  static std::vector<GuiWidgetId> sortedChildIdsByZ(const GuiWidgetTree& tree,
                                                    const GuiWidget& parent);

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Recursive draw-order visitor (parent then z-sorted children).
  static void visitDrawOrderRec(const GuiWidgetTree& tree, GuiWidgetId id,
                                const GuiWidgetVisitor& visitor);


  /// Resolve a tree hit-test result to a mutable widget pointer.
  static GuiWidget* resolveTreeHit(GuiWidgetTree& tree, float x, float y);

  /// Try to cast a widget to GuiTextInput if it contains the point.
  static GuiTextInput* asTextInputAt(GuiWidget* w, float mx, float my);

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Find the topmost visible GuiTextInput tree node under the cursor.
  static GuiTextInput* findTreeTextInput(GuiWidgetTree& tree, GuiWidgetId id,
                                         float mx, float my);


  /// Track the tooltip the pointer is over, now on @p hovered.
  void trackTooltip(GuiWidgetId hovered);
  /// Draw the tooltip, when the pointer has rested long enough.
  void renderTooltip(const GuiDrawContext& ctx) const;

  /// The overlay layer, once made.
  GuiWidgetId overlay_layer_ = GUI_WIDGET_ID_INVALID;
  /// Whether the layout under way is `updateLayout`'s, which skips what
  /// has not changed.
  bool incremental_layout_ = false;
  /// The widget whose tooltip the pointer is resting on, or invalid.
  GuiWidgetId tooltip_target_ = GUI_WIDGET_ID_INVALID;
  /// Seconds the pointer has rested there.
  float tooltip_seconds_ = 0.0f;
  /// Whether a press put its tooltip away until the pointer moves on.
  bool tooltip_dismissed_ = false;
  /// Widget that captured mouse after a successful mouse-down dispatch.
  GuiWidget* captured_ = nullptr;
  /// Text field receiving IME/text input when focused.
  GuiTextInput* focused_input_ = nullptr;
  /// Widget hit on mouse-down when `handleMouseDown` returned false.
  GuiWidget* pending_click_target_ = nullptr;
  /// Button index from that mouse-down; must match for synthesized click.
  GuiMouseButton pending_click_button_ = GuiMouseButton::LEFT;
  /// Text input being drag-selected (set on mouseDown, cleared on mouseUp).
  GuiTextInput* drag_input_ = nullptr;
  /// True if a mouse move occurred during the current drag.
  bool drag_moved_ = false;
  /// Monotonic time accumulated from updateAll dt for double-click timing.
  float elapsed_time_ = 0.0f;
  /// Elapsed time of the last click (for double-click detection).
  float last_click_time_ = -1.0f;
  /// X coordinate of the last click (for double-click proximity check).
  float last_click_x_ = 0.0f;
  /// Y coordinate of the last click (for double-click proximity check).
  float last_click_y_ = 0.0f;
};

}  // namespace eng
