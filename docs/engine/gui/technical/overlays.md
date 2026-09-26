# Simplish — Overlays: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md §5.1](../gui.md#51-pop-up-dialogs)
**Library:** engine (`src/engine/gui/`)
**Date:** 2026-09-25

Anything that floats over the interface goes in the tree's **overlay layer**: menus, context menus, popovers, tooltips, dialogs and toasts. The layer is an ordinary widget, so overlays take part in layout, hit testing, focus navigation, theming and rendering like everything else. They just sit on top.

| File | What it does |
|---|---|
| `GuiWidgetTree::overlayLayer()` (`gui-widget-tree-overlay.cpp`) | The layer: a see-through panel over the whole root, `z_index = GUI_OVERLAY_LAYER_Z`, `pointer_through`, made on first call |
| `gui-popover-placement.h` | `placePopover(anchor, size, viewport, placement)`: where a popover goes, flipped and slid to stay on screen |
| `GuiWidget::tooltip` | A widget's tooltip, drawn by the tree after the pointer rests `GUI_TOOLTIP_DELAY_SECONDS` |
| `gui-modal.h`, `gui-card.h` | A dialog: dims and blocks everything under it, holds focus, centres a card |
| `gui-toasts.h` | Brief notices stacked in the corner that fade |
| `GuiDropdown::popUp` | A menu or context menu opened against an anchor |
| `gui-menu-bar.h` | A bar of menu titles whose menus open in the layer |
| `GuiWidgetTree::needsLayout()` | True when widgets were added or removed since the last layout |
| `test/test_gui_{popover_placement,widget_tree_overlay,modal,toasts,menu_bar}.cpp` | The spec |

## 1. The layer

```
root (the window)
 ├─ … the interface …
 └─ overlay layer   ABSOLUTE inset 0, z = 1 000 000, pointer_through
     ├─ popovers    MANUAL: placed with placePopover
     ├─ modal       ABSOLUTE inset 0: the scrim, card centred
     └─ toasts      ABSOLUTE inset 0, pointer_through, draws its own
```

- It covers the root and is drawn and hit **after** everything else under it.
- It is `pointer_through`, so a click where no overlay is goes to the interface below.
- Its children are placed by hand (`PositionMode::MANUAL` plus `placePopover`) or by insets (`ABSOLUTE`). Insets are laid out by the next `computeLayout`, and adding a widget sets `needsLayout()` so the host knows to run it. The editor's chrome checks this each tick.

## 2. Placement

```cpp
const Rect r = placePopover(button.rect, {240, 180}, window,
                            {.side = GuiPopoverSide::BELOW,
                             .align = Align::START, .gap = 4, .margin = 8});
```

The popover goes on the preferred side. It flips to the opposite side if it does not fit there and the other side has more room. It then slides along both axes to stay inside the viewport less the margin. Every popover in the GUI is placed by this one function: tooltips, `GuiDropdown::popUp` and `GuiMenuBar`.

## 3. Tooltips

```cpp
save_button->tooltip = "Save the level (Cmd+S)";
```

The tree tracks the nearest ancestor-or-self of the hovered widget that has a tooltip. After a 0.5 s rest it draws a small box under that widget, placed by `placePopover`, in the theme's menu look and the CAPTION font. A press hides it until the pointer moves on. No widget is created for it.

## 4. Dialogs

```cpp
auto& modal = *dynamic_cast<GuiModal*>(tree.findWidget(
    tree.insertExternalWidget(std::make_unique<GuiModal>(), tree.overlayLayer())));
const GuiWidgetId card = modal.addCard(tree, 360.0f);   // padded, gap 12, HIGH shadow
// title, text, and a row of buttons under `card` …
modal.dismiss = GuiModalDismiss::CANCEL_ONLY;           // or BACKDROP_OR_CANCEL, NEVER
modal.on_dismiss = [&] { modal.close(tree); };
modal.open(tree);                                       // fades in, scopes focus
```

- While open, the modal covers its parent with the theme's `scrim` and takes every press, so nothing beneath can be reached.
- It keeps focus navigation inside itself (`setFocusScope`), which makes it usable with a pad. `close` releases the scope.
- A click on the backdrop, or CANCEL, calls `on_dismiss` as `dismiss` allows. A click on the card does not.
- `GuiCard` is useful anywhere: a container in the theme's card look, with an `elevation` override.

## 5. Toasts

```cpp
toasts.show("Saved Transit Station", GuiToastKind::SUCCESS);
toasts.show("Build failed — see build/logic.log", GuiToastKind::ERROR, 8.0f);
```

Toasts stack bottom-right, newest lowest, at most `max_shown`. Each fades and slides in, stays for its time, and fades out. The accent rule down its side is coloured by kind: `primary`, `success`, `warning`, `danger`. The widget draws them itself, so showing one needs no layout, and it lets the pointer through. The editor's `SimplishEditor::notify` uses them for outcomes: saved, save failed, build and deploy results, import failures.

## 6. Menus

```cpp
// A context menu at the pointer: a zero-size anchor.
menu->items = {{.label = "Duplicate", .on_select = dup}, {.separator = true},
               {.label = "Delete", .on_select = del}};
menu->tree_layout.position = PositionMode::MANUAL;   // in the overlay layer
menu->popUp({event.x, event.y, 0, 0}, window, {.gap = 0});

// A menu bar.
bar.menus = {{"File", {{.label = "Save", .on_select = save}}},
             {"Edit", {{.label = "Undo", .on_select = undo}}}};
bar.build(tree);
```

`GuiMenuBar` makes ghost-button titles in its row, and a dropdown per menu plus a transparent scrim in the overlay layer. While a menu is open, the scrim catches the click that closes it. It also watches the pointer, so moving onto another title opens that menu. A row's `on_select` runs after the menu closes. The editor keeps its own `EditorMenuBarWidget`, which predates this one and wires the editor's commands and check marks; a new tool or game would use `GuiMenuBar`.

## 7. What replaced what

Overlays used to be *registered components*: widgets outside the tree, listed with `registerComponent`, with their own hit testing, focus and draw paths. That mechanism is gone. An overlay is now a child of the overlay layer, and every tree feature applies to it.
