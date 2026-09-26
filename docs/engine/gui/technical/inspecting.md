# Simplish — Inspecting a Layout: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Library:** engine (`src/engine/gui/`), editor (`src/editor/shell/`, `src/editor/agent/`)
**Date:** 2026-09-25

Three ways to see what layout did, without guessing from code:
- the **layout overlay**, a browser-inspector view drawn in the window;
- **`get_widgets`**, the editor's widget tree as JSON for agents;
- the **widget gallery**, every widget drawn in both themes.

| File | What is in it |
|---|---|
| `gui-layout-overlay.h`, `src/gui-layout-overlay.cpp` | `GuiLayoutOverlay`, `GuiWidgetTree::layout_overlay`, `inspectedWidget()`, and the drawing |
| `src/editor/shell/src/editor-widgets-json.cpp` | `editorWidgetsJson`: the tree as `get_widgets` answers |
| `test/test_gui_layout_overlay.cpp`, `test_gui_gallery_capture.cpp`, `src/editor/shell/test/test_editor_widgets_json.cpp` | The executable spec |

---

## 1. The layout overlay

```cpp
tree.layout_overlay = GuiLayoutOverlay::BOXES;
```

`renderAll` then draws, over everything:

- every visible widget's border box, as a faint cyan hairline;
- for the **deepest widget under the pointer** (`tree.inspectedWidget()`),
  its whole box model, in a browser inspector's colours:

| Tint | Region |
|---|---|
| Orange | Margin |
| Yellow edge | Border box: the widget's `rect` |
| Green | Padding |
| Blue | Content box |

It also draws a tag above it with its `id` (or debug name) and its size in
layout pixels.

In the editor, choose **View › Show Layout Bounds**, or have an agent run
`run_command {"command": "toggle_layout_bounds"}`. The setting lasts for
the session only.

What to look for:

| You see | It means |
|---|---|
| No green where you expected space | The padding is on the parent, not this widget, or the reverse |
| Orange on one side only | A margin, or an `auto` margin taking the free space (`margin_auto`) |
| A hairline far larger than what is drawn | The widget stretches (`align_items: STRETCH` is the default), or grows. Set `align_self` or `flex_grow` |
| A child's box past its parent's | Overflow: a fixed size larger than the room, or `flex_shrink = 0` |

## 2. `get_widgets`: the tree for an agent

```json
POST /tools/get_widgets  {"under": "editor-toolbar", "depth": 2}
{
  "widgets": {"type": "custom", "id": "", "name": "editor-toolbar",
              "rect": [0, 58, 1280, 40], "visible": true, "disabled": false,
              "selected": false, "focused": false, "hovered": false,
              "children": [ ... ]},
  "count": 14, "error": ""
}
```

- `under` is a widget's `id` or debug name. The lowest-numbered one wins
  when several match. Leave it out for the whole window.
- `depth` defaults to 6. A widget at the limit reports `more`, its number of
  children.
- `hidden: true` also lists hidden widgets.
- Rects are in **layout pixels**: the window's size divided by the
  interface scale. They are the rects that clicks and `placePopover` use.

Use it to check what a change did to the editor's chrome: a panel's width,
a button's place, which tool is `selected`. For a game's own screens,
`render_ui_screen` and `get_playtest`'s `ui.nodes` answer the same
questions ([ui.md §6](../../../game/ui.md#6-for-agents)).

Give widgets you will want to find an `id` or a `debug_name`. The editor's
regions already have debug names: `editor-root`, `editor-title-bar`,
`editor-menu-bar`, `editor-toolbar`, `editor-work-row`, `editor-stage`,
`editor-viewport`, `editor-properties` and `editor-asset-browser`.

## 3. The widget gallery

`test_gui_gallery_capture.cpp` builds every widget in a flex page and draws
it with the software rasterizer. It covers:
- type roles and button variants;
- checkbox, toggle and radio;
- text and number fields and a slider;
- tabs and progress;
- a card.

It draws the page three times and writes three gitignored captures at the
repository root:

```
gui-gallery-dark-capture.png
gui-gallery-light-capture.png
gui-layout-overlay-capture.png    the dark gallery with the overlay on, inspecting its card
```

```bash
ctest --preset debug -R gallery
```

Look at both after changing how anything is drawn: a palette, a widget's
`render`, the shape shader, text. The gallery found a text field that
clipped its own text the day it was written. When adding a widget, add it
to the gallery.
