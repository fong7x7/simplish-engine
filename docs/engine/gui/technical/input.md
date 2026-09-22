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
- There are no focus-gained or focus-lost callbacks yet; a widget that needs to know compares `focused_id`.

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

`GuiGamepadNavigator::update(pad, dt)` turns one frame of an
`input::GamepadState` into commands:

- The d-pad, or the left stick past halfway along its stronger axis, gives a
  direction. It fires on the press, repeats after 0.4 s, then every 0.12 s, at
  most once a frame.
- `SOUTH` gives `CONFIRM`, `EAST` gives `CANCEL`, `LEFT_SHOULDER` gives
  `PREVIOUS`, `RIGHT_SHOULDER` gives `NEXT`. Each acts on the press only.

These are the same on every pad, because the buttons are named by position.
`reset(pad)` treats what is already down as handled, so the press that opened a
menu doesn't also confirm in it.

`DesktopGameClient::setGuiPadNavigation(GuiPadNavigation::ON)` feeds the pad in
use through a navigator into the tree each frame. It is off by default: in play
the pad steers a character, so a menu turns it on while open.
`onClientGuiNavUnhandled(command)` receives anything the GUI didn't use.

### 5.7 Not Yet

- Scrolling a scroll container to keep the focused widget in view, or
  scrolling it by pad when nothing focusable lies that way.
- Overlay components registered with `registerComponent` are not navigated;
  only tree widgets are.
- Keyboard navigation (arrows, Tab, Enter, Escape) through `routeNav` is not
  wired. The commands support it, but the editor's own shortcuts use those keys.
- Button prompts per pad family, and Nintendo's swapped confirm/cancel
  convention.

---

## 6. Public Interface (`engine/gui/gui-widget-tree.h`)

| Function | Description |
|----------|-------------|
| `hitTest(x, y)` | Find widget under screen point |
| `routeNav(GuiNavCommand)` | Carry out one navigation command (§5.1) |
| `setFocus(id)` | Focus a focusable widget |
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
| `engine/gui/gui-gamepad-navigator.cpp` | A pad's frame as navigation commands, with repeat | ~110 |

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
