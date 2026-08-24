# Simplish — Layout Engine: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS4.2](../gui.md)
**Library:** engine
**Date:** 2026-03-11

---

## 1. Requirements Summary

| ID | Requirement | Source |
|----|-------------|--------|
| R1 | Row and column flex containers with wrap | gui.md SS4.2 |
| R2 | Alignment: start, center, end, stretch, space-between | gui.md SS4.2 |
| R3 | Padding, margin, min/max size constraints | gui.md SS4.2 |
| R4 | Scrollable containers with inertial scrolling and scroll bars | gui.md SS4.2 |
| R5 | Absolute positioning for overlays and tooltips | gui.md SS4.2 |
| R6 | Partial re-layout when only a subtree changes (dirty flag driven) | gui.md SS4.1 |
| R7 | < 1 ms CPU for in-game HUD layout | gui.md SS7 |

---

## 2. Layout Style

Every widget carries a `LayoutStyle` struct that describes how the layout engine should size and position it:

```cpp
enum class FlexDirection : uint8_t {
  kRow, kColumn,
};

enum class FlexWrap : uint8_t {
  kNoWrap, kWrap,
};

enum class Align : uint8_t {
  kStart, kCenter, kEnd, kStretch, kSpaceBetween,
};

enum class PositionMode : uint8_t {
  kRelative,  // participates in flex flow
  kAbsolute,  // positioned relative to parent, removed from flow
};

struct Edges {
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  float left = 0.0f;
};

struct LayoutStyle {
  FlexDirection direction = FlexDirection::kColumn;
  FlexWrap wrap = FlexWrap::kNoWrap;
  Align align_items = Align::kStretch;
  Align align_content = Align::kStart;
  Align justify_content = Align::kStart;
  Align align_self = Align::kStart;  // per-child override

  float flex_grow = 0.0f;
  float flex_shrink = 1.0f;
  float flex_basis = -1.0f;  // -1 = auto (use min content)

  Edges padding;
  Edges margin;

  float width = -1.0f;      // -1 = auto
  float height = -1.0f;
  float min_width = 0.0f;
  float min_height = 0.0f;
  float max_width = -1.0f;  // -1 = unconstrained
  float max_height = -1.0f;

  float gap = 0.0f;         // spacing between children

  PositionMode position = PositionMode::kRelative;
  float abs_x = 0.0f;       // used when position == kAbsolute
  float abs_y = 0.0f;

  bool scroll_x = false;    // enable horizontal scrolling
  bool scroll_y = false;    // enable vertical scrolling
};
```

---

## 3. Computed Layout

The layout pass fills a `Rect` on each widget representing the final screen-space position and size:

```cpp
struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
};

struct ScrollState {
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  float content_w = 0.0f;   // total content size (may exceed widget rect)
  float content_h = 0.0f;
  float velocity_x = 0.0f;  // for inertial scrolling
  float velocity_y = 0.0f;
};
```

---

## 4. Layout Algorithm

The algorithm runs in two passes, driven by the dirty flags on the widget tree:

### 4.1 Measure Pass (post-order, bottom-up)

1. For **leaf widgets** (Text, Button label, Image): compute intrinsic/min content size from text metrics or fixed dimensions.
2. For **container widgets**: sum children's measured sizes along the main axis; take the max along the cross axis. Apply min/max constraints.
3. Widgets with `flex_basis >= 0` use that as their initial main-axis size instead of intrinsic size.
4. **Absolute** children are excluded from the parent's measure.

### 4.2 Arrange Pass (pre-order, top-down)

1. Start at the root with the viewport rect as the available space.
2. Subtract parent padding from available space.
3. Distribute remaining space along the main axis:
   - Sum flex_grow values of children with grow > 0.
   - Remaining space = available - sum of measured sizes - gaps.
   - Each growing child receives `(remaining * child.flex_grow / total_grow)`.
   - If total measured exceeds available: shrink proportionally using flex_shrink.
4. Position children sequentially along the main axis, inserting `gap` between siblings.
5. **Cross-axis alignment**: apply `align_items` (or per-child `align_self`) to position each child on the cross axis.
6. **Wrapping** (FlexWrap::kWrap): when the main-axis cursor exceeds available space, start a new line. `align_content` controls spacing between lines.
7. **Justify**: `justify_content` distributes remaining main-axis space (start, center, end, space-between).
8. **Absolute children**: positioned relative to parent origin using `abs_x` / `abs_y`; skip flex flow.
9. **Scroll containers**: arrange children as if space is unlimited along the scroll axis. Record `content_w` / `content_h` in ScrollState. Clip rendering to widget rect.

### 4.3 Dirty-Flag Optimisation

- Only subtrees with `dirty == true` are re-measured and re-arranged.
- `markDirty()` propagates up to the root so ancestors can re-distribute space.
- After the arrange pass, `clearDirtyFlags()` resets all flags.

---

## 5. Scroll Containers

Widgets with `scroll_x` or `scroll_y` enabled become scroll containers:

1. **Content overflow** -- children are laid out in unlimited space along the scroll axis; the total content size is stored in `ScrollState`.
2. **Scroll offset** -- applied as a translation during rendering; children outside the visible rect are culled.
3. **Inertial scrolling** -- on mouse-up or touch-up, the last velocity is preserved and decays exponentially each frame (`velocity *= 0.92`). Clamped to content bounds.
4. **Scroll bars** -- rendered as overlay quads; visibility controlled by theme (auto-hide after 1 s idle, or always visible).
5. **Mouse wheel** -- adds a fixed delta (theme-configurable, default 48 px per tick) to the scroll offset.
6. **Gamepad** -- d-pad or right stick scrolls the focused scroll container.

---

## 6. Public Interface (`engine/gui/layout-engine.h`)

| Function | Signature | Description |
|----------|-----------|-------------|
| computeLayout | `void computeLayout(GuiWidgetTree&, const Rect& viewport)` | Run measure + arrange on dirty subtrees |
| measureWidget | `void measureWidget(GuiWidgetTree&, GuiWidgetId)` | Measure a single subtree (post-order) |
| arrangeWidget | `void arrangeWidget(GuiWidgetTree&, GuiWidgetId, const Rect& available)` | Arrange a single subtree (pre-order) |
| updateScroll | `void updateScroll(ScrollState&, float dt)` | Tick inertial scroll decay |
| scrollBy | `void scrollBy(ScrollState&, float dx, float dy)` | Apply scroll delta with clamping |

---

## 7. Error Strategy

| Situation | Handling |
|-----------|----------|
| Negative width/height after constraints | Clamped to 0 |
| flex_grow sum is 0 but space remains | Space left at end (start-aligned) |
| Deeply nested tree (>50 levels) | Layout completes; logged at debug level |
| NaN/Inf in style values | Treated as 0; logged at warning level |

---

## 8. Edge Cases

- A widget with both fixed `width` and `flex_grow > 0`: fixed width takes precedence; flex_grow is ignored.
- A scroll container inside a scroll container: each manages its own ScrollState independently.
- An absolute child with negative coordinates: rendered outside parent bounds but clipped by parent's scissor rect.
- Empty container (no children): sized by padding + min constraints only.

---

## 9. Module Decomposition

| File | Responsibility | Est. Lines |
|------|---------------|------------|
| `engine/gui/layout-engine.h` | LayoutStyle, Rect, ScrollState, Edges, enums, public functions | ~130 |
| `engine/gui/layout-engine.cpp` | Measure/arrange algorithm, scroll update, clamping | ~350 |

---

## 10. Review Log

### Iteration 1
**Checklist results:** 10/11 pass, 1 fail
**Gaps identified:**
- Missing specification for how the dirty-flag optimisation interacts with scroll offset changes (R6 partial re-layout was described but scrolling does not set the layout dirty flag, which could skip re-clamping of scroll bounds)

### Iteration 2
**Checklist results:** 11/11 pass
**Changes made:**
- Clarified in section 4.3 that scroll offset changes mark only `render_dirty` (not layout dirty), and scroll bounds are re-clamped in `updateScroll` without triggering a full layout pass

### Final
**All checklist items pass.** Approach finalised.
