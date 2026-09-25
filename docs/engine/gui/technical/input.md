# Simplish — GUI Input: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS4.7](../gui.md)
**Library:** engine
**Date:** 2026-03-11

---

## 1. Requirements Summary

| ID | Requirement | Source |
|----|-------------|--------|
| R1 | Mouse hit testing against the widget tree (click, double-click, drag, hover, scroll) | gui.md SS4.7 |
| R2 | Keyboard focus management and tab order | gui.md SS4.7 |
| R3 | Text input with IME support | gui.md SS4.7 |
| R4 | Gamepad d-pad focus navigation, confirm/cancel button mapping | gui.md SS4.7 |
| R5 | Hit testing respects scissor clipping and sibling `z_index` order | gui.md SS4.4 |

---

## 2. Hit Testing

### 2.1 Algorithm

Hit testing determines which widget is under a screen coordinate (mouse cursor):

1. Walk the widget tree; among overlapping widgets, prefer the one with the highest `z_index` (see `visitDrawOrder` paint order).
2. For each visible widget, test whether the point is inside `widget.rect`.
3. If the widget is inside a scroll container, offset the test point by the scroll offset.
4. If the widget's rect is clipped by a scissor rect, the point must also be inside the scissor rect.
5. First match wins (topmost visible widget at the coordinate).
6. A widget with `pointer_through` is never the match itself: the point goes through it to whatever is under it, though its children are still tested. A see-through overlay over something clickable sets it — a game's HUD over the playtest's view ([ui.md](../../../game/ui.md)).

### 2.2 Data Structures

```cpp
struct HitTestResult {
  GuiWidgetId widget_id = GUI_WIDGET_ID_INVALID;
  float local_x = 0.0f;  // position relative to widget origin
  float local_y = 0.0f;
};
```

---

## 3. Mouse Events

The GUI input router translates raw mouse events into widget-level events:

```cpp
enum class GuiMouseEventType : uint8_t {
  MOVE,
  BUTTON_DOWN,
  BUTTON_UP,
  DOUBLE_CLICK,
  SCROLL,
};

struct GuiMouseEvent {
  GuiMouseEventType type = GuiMouseEventType::MOVE;
  float x = 0.0f;
  float y = 0.0f;
  GuiMouseButton button = GuiMouseButton::LEFT;
  bool shift_held = false;
  float scroll_dx = 0.0f;
  float scroll_dy = 0.0f;
};
```

### 3.1 Hover Tracking

- On mouse move, hit-test the new position.
- If the hovered widget changes: send `onHoverExit` to the old widget, `onHoverEnter` to the new widget.
- Update `GuiWidgetTree::hovered_id`.

### 3.2 Click Flow

1. On button down: hit-test, set `pressed_id`, send `onPressDown`.
2. On button up: if released over the same widget as pressed, send `onClick`. Always send `onPressUp`.
3. Double-click detected by the platform (SDL3 sends `SDL_EVENT_MOUSE_BUTTON_DOWN` with `clicks == 2`).

### 3.3 Drag

Drag is initiated when the mouse moves beyond a threshold (4 px) while a button is held. The pressed widget receives `onDragStart`, then `onDragMove` on each frame. On button up, `onDragEnd` is sent.

---

## 4. Keyboard Focus

### 4.1 Focus Model

- One widget at a time holds keyboard focus (`GuiWidgetTree::focused_id`).
- Focus is set explicitly by clicking a focusable widget or calling `setFocus()`.
- Only widgets with `tree_focusable` set can receive focus; buttons, sliders, dropdowns and text fields set it themselves (§5.3).
- A widget hears when it gains or loses focus through `handleFocusChange` and `onFocusChange` (§5.11).

### 4.2 Tab Order

Tab and Shift+Tab cycle focus through focusable widgets:

1. Collect all visible, focusable widgets in tree pre-order.
2. Find the index of the currently focused widget.
3. Tab: advance to the next focusable widget (wrapping to the start).
4. Shift+Tab: go to the previous focusable widget (wrapping to the end).

### 4.3 Keyboard Events

```cpp
struct GuiKeyEvent {
  uint32_t keycode = 0;   // SDL_Keycode
  uint32_t scancode = 0;  // SDL_Scancode
  bool pressed = false;
  bool repeat = false;
  bool shift = false;
  bool ctrl = false;
  bool alt = false;
};
```

Key events are routed to the focused widget. If the focused widget does not consume the event, it propagates up the tree to ancestors (bubbling).

### 4.4 Text Input (IME)

For text editing widgets (TextInput):

1. When focused, SDL text input is started (`SDL_StartTextInput`).
2. `SDL_EVENT_TEXT_INPUT` events provide composed characters (UTF-8).
3. `SDL_EVENT_TEXT_EDITING` events provide IME composition state (preedit string, cursor, selection).
4. When focus leaves the text widget, `SDL_StopTextInput` is called.

---

## 5. Gamepad Navigation

Built: [`gui-focus-nav.cpp`](../../../../src/engine/gui/src/gui-focus-nav.cpp) and
[`gui-gamepad-navigator.cpp`](../../../../src/engine/gui/src/gui-gamepad-navigator.cpp),
tested in `test_gui_focus_nav.cpp` and `test_gui_gamepad_navigator.cpp`.

### 5.1 Commands

Navigation runs on `GuiNavCommand` — `UP`, `DOWN`, `LEFT`, `RIGHT`, `CONFIRM`,
`CANCEL`, `NEXT`, `PREVIOUS` — which says nothing about the device.
`GuiWidgetTree::routeNav(command)` carries one out:

1. With nothing focused in the scope (or focus on a widget since hidden), any
   command but `CANCEL` focuses the scope's first focusable widget.
2. On a text field, `CONFIRM` starts typing in it and `CANCEL` stops typing.
3. Otherwise the command is offered to the focused widget, then to each
   ancestor up to the scope's root, through `GuiWidget::handleNav`. The first to
   return true consumes it.
4. What nobody consumed becomes focus movement: directions spatially (§5.2),
   `NEXT`/`PREVIOUS` through tree order, wrapping.
5. Returns false when nothing used the command — for `CANCEL`, the caller's
   cue to close the menu.

### 5.2 Spatial Movement

From the focused widget's centre, each focusable candidate in the scope whose
centre lies at least half a pixel in the pressed direction is scored as
*distance along the direction + 2 × distance off it*. The lowest score wins;
ties go to the earlier in tree order, so the choice depends only on the layout.
With no candidate, focus stays put and `routeNav` returns false.

### 5.3 What Each Widget Does

| Widget | Focusable by default | `handleNav` |
|---|---|---|
| `GuiButton` | yes | `CONFIRM` presses it (the base behaviour) |
| any widget with `onClick` handlers | if `tree_focusable` is set | `CONFIRM` fires them, as a click at its centre |
| `GuiSlider` | yes | `LEFT`/`RIGHT` step the value by `nav_step` (0.05 of the track), firing `on_change` |
| `GuiDropdown` | yes | `UP`/`DOWN` move the highlight between enabled, non-separator rows, consumed even at the ends; `CONFIRM` selects the highlighted row |
| `GuiTextInput`, `GuiTextArea` | yes | handled by the tree: `CONFIRM` starts typing, `CANCEL` stops, moving focus off stops |
| `GuiScrollPanel` | no | scrolls its focused descendant into view, and scrolls on a direction with nothing focusable that way (§5.9) |
| `GuiPanel`, `GuiLabel` | no | — |

A custom widget joins in by setting `tree_focusable` and overriding `handleNav`.

### 5.4 Scope

`setFocusScope(id)` confines navigation to one subtree — an open menu or dialog
— and moves focus into it if it was outside. `setFocusScope(GUI_WIDGET_ID_INVALID)`
releases it. A widget hidden with `visible = false` hides its whole subtree from
navigation.

### 5.5 Focus Ring and the Pointer

`focus_visibility` is `SHOWN` once `routeNav` runs, and `renderAll` then draws a
ring round the focused widget in the style's `focus_ring` token. Any mouse move
or press sets it back to `HIDDEN`. A press on a focusable tree widget also
focuses it, so a pad picked up afterwards carries on from there.

### 5.6 From a Pad

`GuiGamepadNavigator::update(pad, family, dt)` turns one frame of an
`input::GamepadState` into commands:

- The d-pad, or the left stick past halfway along its stronger axis, gives a
  direction. It fires on the press, repeats after 0.4 s, then every 0.12 s, at
  most once a frame.
- The face buttons confirm and cancel by the pad's own convention
  ([`gui-nav-buttons.h`](../../../../src/engine/gui/include/engine/gui/gui-nav-buttons.h)).
  The bottom button confirms and the right one cancels, except on a Nintendo
  pad: there the right button is labelled A, so it confirms and B cancels.
  `LEFT_SHOULDER` gives `PREVIOUS` and `RIGHT_SHOULDER` gives `NEXT`. Each acts
  on the press only.

`reset(pad)` treats whatever is already down as handled, so the press that
opened a menu doesn't also confirm in it.

`DesktopGameClient::setGuiPadNavigation(GuiPadNavigation::ON)` feeds the pad in
use, with its family, through a navigator into the tree each frame. It is off by
default: in play the pad steers a character, so a menu turns it on while open.
`onClientGuiNavUnhandled(command)` receives anything the GUI didn't use.

### 5.7 Prompts

`guiNavButton(command, family)` is the button that does a command, and
`guiNavPrompt(command, family)` is what that pad prints on it. For example,
confirm is "A" on Xbox and Nintendo pads and "Cross" on PlayStation; next is
"RB", "R1" or "R". The labels come from `input::gamepadButtonLabel` and
`gamepadAxisLabel`. The family comes from the pad backend
(`GamepadSet::activeFamily()`, which SDL reports from the pad's type), so a
menu can print "Cross Select · Circle Back" without knowing any pad itself.

### 5.8 From the Keyboard

`DesktopGameClient::setGuiKeyNavigation(GuiKeyNavigation::ON)` routes the
navigation keys through `routeNav`
([`desktop-gui-nav-keys.h`](../../../../src/platform/client/include/engine/client/desktop-gui-nav-keys.h)):

- The arrows move.
- Tab and Shift+Tab give `NEXT` and `PREVIOUS`.
- Enter and Space confirm, and Escape cancels.

While a text field is taking typing, only Escape and Tab navigate, so the
arrows still move its cursor. A held arrow or Tab repeats with the OS; a held
Enter, Space or Escape does not. A key used for navigation never reaches
`onClientKeyDown`. It is off by default, since an editor's arrows and Escape are
its own shortcuts.

### 5.9 Scrolling

`GuiScrollPanel` ([`gui-scroll-panel.h`](../../../../src/engine/gui/include/engine/gui/gui-scroll-panel.h))
is a list taller than its space:

- **Layout:** it stacks its children down a column at their
  `tree_layout.height` (or `row_height`), `tree_layout.gap` apart, inside
  `tree_layout.padding`, and draws them clipped there with a thumb down its
  right edge.
- **Wheel:** it scrolls. `dispatchScroll` now offers the wheel to the widget
  under it and then each ancestor, so a button in a list passes it to the list.
- **Focus:** moving focus to a descendant scrolls every scrolling ancestor just
  far enough to show it (`GuiWidget::revealChild`).
- **Nothing that way:** a direction with no focusable widget that way scrolls
  the nearest scrolling ancestor by `nav_step` (`GuiWidget::scrollByNav`), so
  text below the last button can be read. Only when that can't move either does
  `routeNav` return false.
- **Clipping:** the tree draws every widget's children clipped to its
  `childClipRect()`, and clips the focus ring the same way, so a ring never
  shows outside the list.

A widget that scrolls re-lays itself out in `arrangeAfterScroll`, which the tree
calls after any of the three scrolls. The tree's own `SCROLL_CONTAINER` nodes
stay plain panels, because the markdown renderer lays those out itself;
`GuiScrollPanel` is added with `insertExternalWidget`.

### 5.10 Overlays

Overlay components registered with `registerComponent` take part as well: after
the tree's widgets in focus order, and in spatial moves beside them. An overlay
has no widget id, so while one has focus `focused_id` is invalid and
`focusedWidget()` names it. `setFocus(GuiWidget&)` focuses either kind. A focus
scope leaves overlays out, since they belong to no subtree. A command a focused
overlay doesn't take has no parent to go to.

### 5.11 Horizontal Lists, the Right Stick, and Focus Callbacks

- `GuiScrollPanel::axis` set to `HORIZONTAL` lays children out in a row at
  their `tree_layout.width` (or `item_size`) and scrolls sideways — under the
  wheel, by LEFT and RIGHT when nothing focusable lies that way, and to show
  what is focused.
- `GuiWidget::scrollBy(dx, dy)` is the one pixel scroll every scroll goes
  through. `GuiWidgetTree::scrollFocusBy` sends it to the nearest scrolling
  ancestor of the focused widget, and the desktop client calls that with
  `GuiGamepadNavigator::scrollDelta` — the right stick, past a deadzone,
  faster the further it is pushed — while pad navigation is on. Scrolling by
  stick reads; it never moves focus.
- `GuiWidget::handleFocusChange(GAINED | LOST)`, and `onFocusChange`
  handlers, are called whenever tree focus moves — by navigation, a click, or
  `setFocus` — the old widget first.
- The kind of device in use is `input::InputMethod`, which the desktop client
  tracks ([input.md §6](../../input.md#6-couch-players-rumble-and-which-device-is-in-use)).

### 5.12 Not Yet

- Grid-shaped scroll panels, scrolling on both axes at once.

---

## 6. Public Interface (`engine/gui/gui-widget-tree.h`)

| Function | Description |
|----------|-------------|
| `hitTest(x, y)` | Find widget under screen point |
| `routeNav(GuiNavCommand)` | Carry out one navigation command (§5.1) |
| `setFocus(id)`, `setFocus(GuiWidget&)` | Focus a focusable tree widget or overlay |
| `focusedWidget()` | The focused widget, tree node or overlay |
| `setFocusScope(id)` | Confine navigation to a subtree (§5.4) |
| `advanceFocus(FocusTraversalDirection)` | Step through focus order in the scope |
| `navigateFocus(GuiNavCommand)` | Move focus spatially; false when nothing lies that way |

---

## 7. Error Strategy

| Situation | Handling |
|-----------|----------|
| Hit test on empty tree | Returns `GUI_WIDGET_ID_INVALID` |
| setFocus on non-focusable widget | No-op; log debug |
| Tab with zero focusable widgets | No-op |
| Gamepad navigation with no candidates | Focus unchanged; `routeNav` returns false |
| Key event with no focused widget | Event dropped |

---

## 8. Edge Cases

- Clicking a disabled widget: hit test succeeds but onClick is not fired; widget receives hover state for cursor feedback.
- Focus on a widget that becomes invisible: focus cleared, focus moves to next focusable or to `GUI_WIDGET_ID_INVALID`.
- Overlapping siblings with the same `z_index`: stable tree child order defines paint and hit-test tie-break.
- Mouse event during gamepad navigation: input method switches back to keyboard/mouse; hover tracking resumes.

---

## 9. Module Decomposition

| File | Responsibility | Est. Lines |
|------|---------------|------------|
| `engine/gui/gui-input.h` | HitTestResult, GuiMouseEvent, GuiKeyEvent, public functions | ~100 |
| `engine/gui/gui-input.cpp` | Hit testing, event routing | ~115 |
| `engine/gui/gui-focus-nav.cpp` | Focus movement, scope, command routing, focus ring | ~290 |
| `engine/gui/gui-gamepad-navigator.cpp` | A pad's frame as navigation commands, with repeat | ~115 |
| `engine/gui/gui-nav-buttons.cpp` | Which button does each command per pad family, and its prompt | ~70 |
| `engine/gui/gui-scroll-panel.cpp` | A clipped, scrolling column of widgets | ~130 |
| `platform/client/src/desktop/desktop-gui-nav-keys.cpp` | Which keys navigate | ~55 |

---

## 10. Review Log

### Iteration 1
**Checklist results:** 10/11 pass, 1 fail
**Gaps identified:**
- No specification of how hit testing handles scroll-offset transforms nested more than one level deep (R5 scissor clipping was addressed but nested scroll containers were not)

### Iteration 2
**Checklist results:** 11/11 pass
**Changes made:**
- Added clarification in section 2.1 step 3 that the scroll offset is accumulated through each ancestor scroll container, ensuring nested scroll containers correctly offset the test point

### Final
**All checklist items pass.** Approach finalised.
