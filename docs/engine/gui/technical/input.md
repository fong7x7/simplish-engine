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
- Only widgets with `focusable == true` can receive focus (TextInput, Button, etc.).
- When focus changes, `onFocusLost` is sent to the old widget and `onFocusGained` to the new widget.

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

### 5.1 D-Pad Focus Navigation

When the active input method is gamepad, d-pad directions move focus spatially:

1. From the currently focused widget, compute its center point.
2. For each candidate focusable widget in the pressed direction:
   - Must be in the correct half-plane (e.g. d-pad right: candidate center x > current center x).
   - Score by angular alignment with the direction vector and distance.
3. Focus moves to the best-scoring candidate.
4. If no candidate exists in the direction, focus stays on the current widget.

### 5.2 Confirm / Cancel

- Gamepad Confirm (A / Cross): triggers `onClick` on the focused widget.
- Gamepad Cancel (B / Circle): triggers `onCancel` -- typically closes the current menu or dialog.

### 5.3 Scroll

When focus is inside a scroll container, gamepad right stick or d-pad (when no focusable widget exists in the direction) scrolls the container.

---

## 6. Public Interface (`engine/gui/gui-input.h`)

| Function | Signature | Description |
|----------|-----------|-------------|
| hitTest | `HitTestResult hitTest(const GuiWidgetTree&, float x, float y)` | Find widget under screen point |
| routeMouseEvent | `bool routeMouseEvent(GuiWidgetTree&, const GuiMouseEvent&)` | Process a mouse event |
| routeKeyEvent | `bool routeKeyEvent(GuiWidgetTree&, const GuiKeyEvent&)` | Process a keyboard event |
| routeTextInput | `bool routeTextInput(GuiWidgetTree&, std::string_view text)` | Process composed text input |
| setFocus | `void setFocus(GuiWidgetTree&, GuiWidgetId)` | Set keyboard focus |
| advanceFocus | `void advanceFocus(GuiWidgetTree&, bool reverse)` | Tab / Shift+Tab focus cycling |
| navigateFocus | `void navigateFocus(GuiWidgetTree&, FlexDirection direction)` | D-pad spatial focus navigation |

---

## 7. Error Strategy

| Situation | Handling |
|-----------|----------|
| Hit test on empty tree | Returns `GUI_WIDGET_ID_INVALID` |
| setFocus on non-focusable widget | No-op; log debug |
| Tab with zero focusable widgets | No-op |
| Gamepad navigation with no candidates | Focus unchanged |
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
| `engine/gui/gui-input.cpp` | Hit testing, focus management, tab order, d-pad navigation, event routing | ~300 |

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
