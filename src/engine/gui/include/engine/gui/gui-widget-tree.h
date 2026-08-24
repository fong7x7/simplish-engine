#pragma once

/// @file gui-widget-tree.h
/// @brief Retained widget tree plus registered overlay `GuiWidget`s (hit
/// test, render, text focus) in one context.
/// @par Threading Main thread only.

#include "adopt-result.h"
#include "gui-draw-context.h"
#include "gui-input.h"
#include "gui-text-input.h"
#include "gui-widget.h"
#include "layout-engine.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace eng {

using GuiWidgetVisitor = std::function<void(const GuiWidget&)>;
using GuiWidgetMutVisitor = std::function<void(GuiWidget&)>;

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

  /// Run measure + arrange; viewport is the screen rect.
  void computeLayout(const Rect& viewport);

  /// Measure a single subtree bottom-up (post-order).
  void measureWidget(GuiWidgetId id);

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

  /// Cycle focus to the next or previous focusable widget.
  void advanceFocus(FocusTraversalDirection direction);

  /// Move focus spatially in a direction (for gamepad d-pad navigation).
  void navigateFocus(FlexDirection direction);

  /// Set shared style for all registered overlay components.
  void setStyle(const GuiStyle& style);

  /// Register an overlay component (non-owning). Among overlays, higher
  /// `z_index` draws and receives hits above lower; ties keep registration
  /// order.
  void registerComponent(GuiWidget& comp);

  void unregisterComponent(GuiWidget& comp);

  /// Unregister all overlay components; clears capture and text focus.
  void clearComponents();

  void updateHover(float mx, float my);

  /// Per-frame tick for every registered overlay and tree widget.
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

  /// Returns the number of registered overlay components.
  [[nodiscard]] size_t componentCount() const;

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

  /// `ord_asc`: overlays in ascending `z_index` (same order as paint).
  void
  applyTextInputFocusFromSortedOverlays(const std::vector<GuiWidget*>& ord_asc,
                                        float mx, float my);

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

  /// Sort overlay components by ascending z_index.
  static std::vector<GuiWidget*>
  sortedOverlaysZAsc(const std::vector<GuiWidget*>& comps);

  /// Return the topmost widget (highest z) that contains (x, y).
  static GuiWidget* topHitInZAscOrder(const std::vector<GuiWidget*>& ord_asc,
                                      float x, float y);

  /// Return the topmost GuiTextInput that contains (mx, my).
  static GuiTextInput*
  topTextInputInZAscOrder(const std::vector<GuiWidget*>& ord_asc, float mx,
                          float my);

  /// Find the topmost overlay component at (x, y).
  static GuiWidget* findTopOverlayAt(const std::vector<GuiWidget*>& comps,
                                     float x, float y);

  /// Hit-test overlays first, then the widget tree.
  GuiWidget* hitTestAny(float mx, float my);

  /// Resolve a tree hit-test result to a mutable widget pointer.
  static GuiWidget* resolveTreeHit(GuiWidgetTree& tree, float x, float y);

  /// Try to cast a widget to GuiTextInput if it contains the point.
  static GuiTextInput* asTextInputAt(GuiWidget* w, float mx, float my);

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Find the topmost visible GuiTextInput tree node under the cursor.
  static GuiTextInput* findTreeTextInput(GuiWidgetTree& tree, GuiWidgetId id,
                                         float mx, float my);

  /// Render overlay components in ascending z_index order.
  static void renderSortedOverlays(const std::vector<GuiWidget*>& comps,
                                   const GuiDrawContext& ctx);

  /// Flat list of widgets participating in hit-testing and focus (scene order).
  std::vector<GuiWidget*> components_{};
  /// Theme tokens resolved for the current paint pass.
  const GuiStyle* active_style_ = nullptr;
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
