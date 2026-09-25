# Simplish — Layout Engine: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md §4.2](../gui.md#42-layout-engine)
**Library:** engine (`src/engine/gui/`)
**Date:** 2026-09-25 (rewritten when the flexbox pass shipped)

The GUI lays widgets out by **CSS flexbox**, in logical pixels. Every widget
carries a `LayoutStyle` (`tree_layout`); `GuiWidgetTree::computeLayout`
measures the tree bottom-up and places it top-down. If you know CSS
flexbox, you already know how this behaves. §3 is the reference, §4 has
recipes to copy, and §8 lists the differences from CSS.

| File | What is in it |
|---|---|
| `include/engine/gui/layout-engine.h` | `LayoutStyle` and its enums: `FlexDirection`, `FlexWrap`, `Align`, `PositionMode` |
| `include/engine/gui/layout-size.h` | `LayoutSize`, what the measure pass produces |
| `include/engine/gui/gui-widget.h` | `tree_layout`, `tree_measured`, `measureContent`, `arrangeChildren` |
| `include/engine/gui/gui-widget-tree.h` | `computeLayout`, `measureWidget`, `arrangeWidget` |
| `src/flex-layout.{h,cpp}` | The algorithm (private) |
| `src/layout-engine.cpp` | The two passes, and the scroll-state helpers |
| `test/test_gui_flex_layout.cpp` | One test per behaviour; the executable spec |

---

## 1. The model in one screen

```
 margin (outside; keeps siblings away — adds to the parent's gap)
┌──────────────────────────────────────────┐
│ border box = widget.rect                 │  width/height, min/max,
│   padding (inside)                       │  tree_measured — all
│  ┌────────────────────────────────────┐  │  border-box sizes
│  │ content box: where children go    │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

- **Sizes are border-box.** `width: 100` with `padding.left: 10` is 100
  wide, of which 90 is content. It is `box-sizing: border-box`, the way
  every modern stylesheet sets it.
- **A widget's `rect` is its border box.** Margins are never inside it.
- **`-1` means auto** for `width`, `height`, `max_*`, `flex_basis`,
  `abs_right` and `abs_bottom`.
- **Defaults match a CSS `display: flex; flex-direction: column`
  container**: children stack top to bottom, stretched across, no grow,
  shrink 1.

## 2. The two passes

```cpp
tree.computeLayout(window_rect, draw_context);  // measure, then arrange
```

1. **Measure** (post-order). Each widget's `tree_measured` is set to its
   *natural* border-box size:
   - an explicit `width` / `height` wins on its axis;
   - otherwise it is the larger of **its own content** (`measureContent`:
     a label's text, one line high) and **its in-flow children** (end to
     end along its main axis with gaps and their margins, and the thickest
     across), **plus its padding**;
   - then clamped to `min_*` / `max_*`.
2. **Arrange** (pre-order). The root is given the whole viewport. Each
   widget's `arrangeChildren` places its children inside its content box,
   calling `tree.arrangeWidget(child, rect)` on each, which sets
   `child.rect` and recurses. The default `arrangeChildren` is the flex
   algorithm (§5).

The `GuiDrawContext` is what text is measured with. Pass the real one
(`RenderedGameClient::guiDrawContext()`); the one-argument
`computeLayout(viewport)` measures text at a fixed 8 px a character and
14 px a line, which is enough for tests.

**When to run it.** `computeLayout` always lays out the whole tree, so run
it when something it depends on changes: window size, a panel folding, a
widget shown or hidden in flow, or a size you set. Do not run it every
frame on a large tree. The editor keeps the inputs from the last pass and
compares them (`SimplishEditor::chromeNeedsLayout`).

Changing `visible` on an in-flow widget changes the flow, so relayout.
Absolute children are placed even while hidden, so showing an overlay needs
no relayout.

## 3. `LayoutStyle` reference

### On a container (how it lays out its children)

| Field | Default | Meaning |
|---|---|---|
| `direction` | `COLUMN` | Main axis: `ROW` (left→right) or `COLUMN` (top→bottom) |
| `wrap` | `NO_WRAP` | `WRAP` starts a new line when the next child would overflow |
| `justify_content` | `START` | Free space along the main axis: `START`, `CENTER`, `END`, `SPACE_BETWEEN`, `SPACE_AROUND`, `SPACE_EVENLY` (`STRETCH` acts as `START`) |
| `align_items` | `STRETCH` | Each child across its line: `START`, `CENTER`, `END`, `STRETCH` |
| `align_content` | `START` | Wrapped lines across the container: any `Align`; `STRETCH` shares the space among lines |
| `gap` | `0` | Between adjacent children *and* between wrapped lines |
| `padding` | `0` | Inset of the content box |

### On a child (how it sits in its parent)

| Field | Default | Meaning |
|---|---|---|
| `width`, `height` | `-1` | Explicit border-box size; `-1` measures it |
| `min_width`, `min_height` | `0` | Never smaller; wins over max |
| `max_width`, `max_height` | `-1` | Never larger |
| `flex_grow` | `0` | Share of the free space on its line, in proportion to siblings |
| `flex_shrink` | `1` | Share of an overflow, weighted by `flex_shrink × base size`; `0` never shrinks |
| `flex_basis` | `-1` | Main size before growing or shrinking; `-1` is the explicit size, else the measured one. **`0` with `flex_grow` means "take only leftover space"** |
| `align_self` | `AUTO` | Overrides the parent's `align_items` for this child |
| `margin` | `0` | Space outside; adds to `gap`, never collapses |
| `position` | `RELATIVE` | `RELATIVE`: in flow. `ABSOLUTE`: out of flow, placed by insets. `MANUAL`: out of flow and never placed (§6) |
| `abs_x`, `abs_y` | `0` | `ABSOLUTE`: distance from the parent's left/top edge to the margin |
| `abs_right`, `abs_bottom` | `-1` | `ABSOLUTE`: distance from the right/bottom edge. With an auto size, stretches from `abs_x`/`abs_y` to here. With an explicit size, anchors to that edge |

`scroll_x` / `scroll_y` are not read by the layout. A scrolling list is a
`GuiScrollPanel` (§6).

## 4. Recipes

Every recipe below is an ordinary tree built with `createWidget` or
`insertExternalWidget`, then `computeLayout`. Each mechanism a recipe
uses has a test in `test_gui_flex_layout.cpp`. Recipes 4.1, 4.3 and 4.7
run in the editor today.

### 4.1 App frame: fixed bars, a growing middle, a fixed footer

```cpp
// root: default column, stretched across the window
auto strip = [&](GuiWidgetId id, float h) {
  auto& s = tree.findWidget(id)->tree_layout;
  s.height = h;
  s.flex_shrink = 0.0f;       // bars never give way on a short window
};
strip(title, 28.0f);
strip(toolbar, 36.0f);

auto& middle = tree.findWidget(work_row)->tree_layout;
middle.direction = FlexDirection::ROW;
middle.flex_grow = 1.0f;
middle.flex_basis = 0.0f;     // only what the bars and footer leave

tree.findWidget(footer)->tree_layout.height = 160.0f;  // shrinks last
```

This is the editor's chrome: `SimplishEditor::initChrome` and
`layoutChrome` in `src/editor/shell/src/simplish-editor.cpp`. Because the
middle row has a zero basis, a short window empties it first. The footer
starts shrinking only when there is no middle left.

### 4.2 Sidebar and main area

```cpp
// in a ROW
tree.findWidget(sidebar)->tree_layout.width = 240.0f;   // shrink 1: gives way
auto& main = tree.findWidget(content)->tree_layout;
main.flex_grow = 1.0f;
main.flex_basis = 0.0f;
main.padding = {16.0f, 16.0f, 16.0f, 16.0f};
```

A sidebar whose width is `0` when it has nothing to show (the editor's
properties panel) simply takes no room. Set its width and relayout.

### 4.3 Toolbar: items on the left, one item pushed to the right

```cpp
bar.direction = FlexDirection::ROW;
bar.align_items = Align::CENTER;             // vertically centred
bar.padding = {0.0f, 10.0f, 0.0f, 10.0f};
bar.gap = 4.0f;
// ... buttons: width 72, height 24, flex_shrink 0 ...
play.margin.left = 16.0f;                    // extra space before one item
spacer.flex_grow = 1.0f;                     // an empty GuiPanel with a
                                             // transparent fill
status.width = 260.0f;                       // shrink 1: the one that gives
```

There are no `auto` margins (§8), so a growing spacer does the pushing.
Pick one item to give way on narrow windows (here the status text) and set
`flex_shrink = 0` on the rest. See `EditorToolbarWidget`.

### 4.4 Button that sizes to its label

```cpp
auto* ok = dynamic_cast<GuiButton*>(tree.findWidget(
    tree.createWidget(GuiWidgetType::BUTTON, row)));
ok->label = "Save changes";
ok->tree_layout.padding = {6.0f, 14.0f, 6.0f, 14.0f};
ok->tree_layout.min_width = 80.0f;
```

`GuiButton` and `GuiLabel` measure their text. Padding adds on top, so
this is the CSS `padding: 6px 14px` button. In a `COLUMN` with the default
`STRETCH` it would span the column; give the column
`align_items = Align::START` (or the button `align_self`) to keep it at
its natural width.

### 4.5 Centred dialog

```cpp
auto& scrim = tree.findWidget(backdrop)->tree_layout;
scrim.position = PositionMode::ABSOLUTE;     // covers the parent
scrim.abs_right = 0.0f;
scrim.abs_bottom = 0.0f;
scrim.justify_content = Align::CENTER;       // a column: centres up/down
scrim.align_items = Align::CENTER;           // and across

auto& box = tree.findWidget(dialog)->tree_layout;
box.width = 420.0f;
box.padding = {20.0f, 24.0f, 20.0f, 24.0f};
box.gap = 12.0f;                              // title, body, button row
```

The dialog's height comes from its contents.

### 4.6 Card grid that wraps

```cpp
grid.direction = FlexDirection::ROW;
grid.wrap = FlexWrap::WRAP;
grid.gap = 12.0f;
grid.align_items = Align::START;
// each card
card.width = 180.0f;
card.flex_shrink = 0.0f;
```

For cards that stretch to fill each row, give them `flex_grow = 1` and a
`min_width` instead of a fixed width. A line's height is its tallest card's.

### 4.7 Overlay covering a widget, and a corner badge

```cpp
// overlay: fills its parent, takes no space from its siblings
overlay.position = PositionMode::ABSOLUTE;
overlay.abs_right = 0.0f;
overlay.abs_bottom = 0.0f;

// badge: 20×20, 6 px in from the top-right corner
badge.position = PositionMode::ABSOLUTE;
badge.width = badge.height = 20.0f;
badge.abs_y = 6.0f;
badge.abs_right = 6.0f;
```

The editor lays its playtest screens (character select, Controls, Sound)
over the viewport this way. They are absolute children of the "stage"
panel that holds the viewport; see `SimplishEditor::coverStage`.

## 5. The arrange algorithm

For one container, in `arrangeFlexChildren`:

1. **Content box.** The border box less the padding.
2. **Collect** the visible `RELATIVE` children. The base size of each is
   its `flex_basis`, or else its measured main size, clamped to its
   min/max.
3. **Break lines** (`WRAP` only). A child starts a new line when
   `used + gap + its outer size` would pass the content width.
4. **Resolve flexible lengths** per line, as CSS §9.7 does. Free space is
   the room minus gaps, margins and base sizes. If positive, unfrozen
   children grow by `flex_grow`. A sum of grow factors below 1 hands out
   only that fraction. If negative, they shrink by `flex_shrink × base`.
   A child its min or max holds back is frozen there, and the rest are
   re-shared, until every child is frozen.
5. **Line cross sizes.** One `NO_WRAP` line is the whole content box.
   Wrapped lines are as thick as their thickest child. `align_content`
   then offsets or stretches them.
6. **Place.** `justify_content` spreads what is left along the main axis
   (negative free space overflows past the end for `START` and
   `SPACE_BETWEEN`, and both ways for `CENTER`). Each child's cross
   position comes from `align_self` or `align_items`. `STRETCH` fills the
   line less the margins, unless the child has an explicit cross size.
7. **Absolute children** are placed by their insets against the
   container's *border* box (the padding does not apply, as in CSS).

## 6. Widgets that lay out their own children

Most widgets need nothing: the default `arrangeChildren` is the flex
layout. There are three ways to do something else:

| You want | Do this | Example |
|---|---|---|
| A leaf with a natural size (text, an image, a glyph) | Override `measureContent(ctx)`. Return the content size **without** padding | `GuiLabel`, `GuiButton`, `GuiTextInput` |
| A container with its own placement rule | Override `arrangeChildren(tree, available)`. Call `tree.arrangeWidget(child, rect)` on each child so its subtree is laid out too. Setting `child->rect` alone does not recurse | `GuiScrollPanel` (a scrolling stack), `GuiDockspaceWidget`, `EditorMenuBarWidget` |
| A widget placed by some other widget, outside its parent's flow | `tree_layout.position = PositionMode::MANUAL`. The layout neither moves it nor recurses into it | The menu bar's dropdowns and scrim: children of the editor's root, positioned by `EditorMenuBarWidget::layout` |

A widget can be laid out on its own, outside `computeLayout`, with
`tree.measureWidget(id, ctx)` followed by `tree.arrangeWidget(id, rect)`.
`EditorToolbarWidget::layout` does this for its tests.

`GuiScrollPanel` keeps its own stacking rule. Its children are placed at
`tree_layout.height` (or `width` sideways), or `item_size`, and are
flex-laid-out inside that slot.

## 7. Testing a layout

Build the tree in a Catch2 fixture, lay it out, and assert on `rect`. No
window or GPU is needed:

```cpp
GuiWidgetTree tree;
const GuiWidgetId root =
    tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
tree.findWidget(root)->tree_layout.direction = FlexDirection::ROW;
const GuiWidgetId a = tree.createWidget(GuiWidgetType::PANEL, root);
tree.findWidget(a)->tree_layout.flex_grow = 1.0f;
tree.computeLayout({0.0f, 0.0f, 400.0f, 300.0f});
REQUIRE(tree.findWidget(a)->rect.w == 400.0f);
```

`test_gui_flex_layout.cpp` has the `FlexFixture` helper. For how it looks,
render through `GuiSoftwareRasterizer` as the editor's `*_capture` tests
do, and open the PNG.

## 8. Differences from CSS

| CSS | Here |
|---|---|
| `min-width: auto` (items will not shrink below their content) | `min_width` defaults to `0`. Set it yourself where text must not be squeezed |
| `margin: auto` | Not supported. Use a `flex_grow` spacer (§4.3) or `justify_content` |
| Percent and `em` units | Pixels only |
| `row-reverse`, `column-reverse`, `order` | Not supported. Order children in the tree |
| Separate `row-gap` / `column-gap` | One `gap` for both |
| Text that wraps to the width it is given | Labels measure one line. Wrapped text (`GuiTextArea`) needs an explicit height |
| `overflow: scroll` on any box | Use `GuiScrollPanel` |
| Baseline alignment | Not supported |

Other behaviour to know about:
- There is no partial relayout. `tree_dirty` is cleared by
  `computeLayout` but not yet used to skip clean subtrees.
- Positions are not pixel-snapped. Fractional rects are possible with
  `CENTER` and `SPACE_*`.
