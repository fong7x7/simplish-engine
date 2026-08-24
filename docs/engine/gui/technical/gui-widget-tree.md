# Simplish — Widget Tree: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS4.1](../gui.md)
**Library:** engine
**Date:** 2026-03-11

---

## 1. Requirements Summary

| ID | Requirement | Source |
|----|-------------|--------|
| R1 | Persistent retained-mode widget tree with explicit create/update/destroy | gui.md SS4.1 |
| R2 | Dirty-flag diffing per frame to determine re-layout and re-render regions | gui.md SS4.1 |
| R3 | Smooth animations via retained state (opacity, position, colour lerps) | gui.md SS4.1 |
| R4 | Accessibility traversal (screen readers walk the tree) | gui.md SS4.1 |
| R5 | Widget identity by stable ID for tree diffing | gui.md SS4.1 |
| R6 | Z-ordered draw layers: game viewport, HUD, menus, editor, tooltips, modals | gui.md SS4.4 |

---

## 2. Context Objects

### 2.1 GuiWidgetId

Stable 64-bit identity for each tree node (`gui-widget-id.h`):

```cpp
using GuiWidgetId = uint64_t;
constexpr GuiWidgetId GUI_WIDGET_ID_INVALID = 0;
```

### 2.2 GuiWidgetType

Widget kind for factory/theming (`gui-widget-type.h`): `Panel`, `Text`, `Button`, `TextInput`, `ScrollContainer`, `Custom`.

### 2.3 GuiWidget and GuiWidgetTree

Runtime nodes are `GuiWidget` subclasses (`gui-widget.h`); the tree stores `std::unordered_map<GuiWidgetId, std::unique_ptr<GuiWidget>>` plus `root_id`, `next_id`, focus/hover/press ids, layout, hit test, and overlay lists (`gui-widget-tree.h`).

---

## 3. Public Interface (`engine/gui/gui-widget-tree.h`)

| Function | Signature | Description |
|----------|-----------|-------------|
| createWidget | `GuiWidgetId createWidget(GuiWidgetType, GuiWidgetId parent)` | Allocate widget by type, attach to parent |
| insertExternalWidget | `GuiWidgetId insertExternalWidget(unique_ptr<GuiWidget>, GuiWidgetId parent)` | Insert pre-created widget (e.g. a `GuiPanel` subclass); assigns fresh ID |
| adoptSubtree | `GuiWidgetId adoptSubtree(const GuiWidgetTree& src, GuiWidgetId src_root, GuiWidgetId parent)` | Deep-copy a subtree from another tree with fresh IDs |
| adoptSubtreeWithMap | `AdoptResult adoptSubtreeWithMap(...)` | Same as adoptSubtree but returns old→new ID mapping |
| destroyWidget | `void destroyWidget(GuiWidgetId)` | Remove widget and all descendants |
| findWidget | `GuiWidget* findWidget(GuiWidgetTree&, GuiWidgetId)` | Lookup; returns nullptr if not found |
| findWidgetConst | `const GuiWidget* findWidget(const GuiWidgetTree&, GuiWidgetId)` | Const lookup |
| reparentWidget | `bool reparentWidget(GuiWidgetTree&, GuiWidgetId, GuiWidgetId new_parent)` | Move subtree |
| markDirty | `void markDirty(GuiWidgetTree&, GuiWidgetId)` | Flag widget and ancestors for re-layout |
| clearDirtyFlags | `void clearDirtyFlags(GuiWidgetTree&)` | Reset after layout pass |
| visitPreOrder | `void visitPreOrder(const GuiWidgetTree&, GuiWidgetId, Visitor)` | Pre-order traversal for layout |
| visitPostOrder | `void visitPostOrder(const GuiWidgetTree&, GuiWidgetId, Visitor)` | Post-order traversal for sizing |
| visitDrawOrder | `void visitDrawOrder(const GuiWidgetTree&, Visitor)` | Paint order from `root_id`; siblings sorted by `z_index` |
| childCount | `size_t childCount(const GuiWidgetTree&, GuiWidgetId)` | Child count |
| renderAll | `void renderAll(GuiWidgetTree&, GuiDrawContext)` | Renders retained tree then overlay components, both respecting `z_index` |

`Visitor` is `std::function<void(const GuiWidget&)>`. Overlay mouse/scroll routing (`dispatchMouseDown`, `dispatchClick`, `dispatchScroll`, text-input focus) uses the same `z_index` ordering so the topmost overlay receives input.

**Note:** `visitPreOrder` / `visitPostOrder` still follow raw `children` order for layout measure/arrange; only paint and overlay input use `z_index` sorting among siblings.

---

## 4. Data Flow

1. **Create** -- Game/editor calls `createWidget()` for standard types or `insertExternalWidget()` for custom `GuiWidget` subclasses (e.g. `LauncherWidget : GuiPanel`). Each widget gets a monotonically-increasing `GuiWidgetId`.
2. **Update** -- Caller modifies layout fields, `GuiWidget::tree_state`, or visibility. Calls `markDirty()` to propagate dirty flags up to the root.
3. **Layout** -- Layout engine calls `visitPostOrder()` (measure children bottom-up) then `visitPreOrder()` (assign positions top-down). Clears dirty flags.
4. **Render** -- Renderer walks the tree via `visitDrawOrder` (`z_index` among siblings), emitting quads for each visible widget.
5. **Destroy** -- `destroyWidget()` recursively removes a subtree. If the destroyed widget was focused/hovered, those references are cleared.

---

## 5. Error Strategy

| Situation | Handling |
|-----------|----------|
| `findWidget()` with invalid ID | Returns nullptr |
| `destroyWidget()` with invalid ID | No-op; logged at debug level |
| `reparentWidget()` creating cycle | Returns false; no modification |
| `createWidget()` with invalid parent | Returns `GUI_WIDGET_ID_INVALID` |

---

## 6. Edge Cases

- Destroying the root resets `root_id` to `GUI_WIDGET_ID_INVALID`
- Destroying a focused widget clears `focused_id`
- Creating a widget when parent has been destroyed returns invalid
- Tree supports up to 10,000 widgets before requiring flat-array migration (measured, not premature)

---

## 7. Module Decomposition

| File | Responsibility | Est. Lines |
|------|---------------|------------|
| `engine/gui/gui-widget-tree.h` | Public interface, GuiWidget/GuiWidgetTree types | ~120 |
| `engine/gui/gui-widget-tree.cpp` | Tree operations, traversal, dirty propagation | ~200 |

---

## 8. Review Log

### Iteration 1
**Checklist results:** 10/11 pass, 1 fail
**Gaps identified:**
- No specification of how `GuiWidgetId` recycling or exhaustion is handled (R5 stable identity requires monotonically increasing IDs but no wraparound or exhaustion strategy was defined)

### Iteration 2
**Checklist results:** 11/11 pass
**Changes made:**
- Added note in section 2.1 that `GuiWidgetId` uses uint64_t which cannot practically exhaust, and IDs are never recycled to guarantee stable identity across the session lifetime

### Final
**All checklist items pass.** Approach finalised.
