# Simplish — Widgets (M1): Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS5](../gui.md)
**Library:** engine
**Date:** 2026-03-11

---

## 1. Requirements Summary

| ID | Requirement | Source |
|----|-------------|--------|
| R1 | Panel: container with optional title bar, background, border, shadow, rounded corners | gui.md SS5 |
| R2 | Text: static label with rich-text attributed spans | gui.md SS5 |
| R3 | Button: clickable with label, icon, hover/active/disabled states | gui.md SS5 |
| R4 | TextInput: single-line editable field with placeholder, cursor, selection, IME | gui.md SS5 |
| R5 | ScrollContainer: scrollable area with vertical/horizontal scroll bars, inertial scrolling | gui.md SS5 |

---

## 2. Widget State

All M1 widgets share a common interaction state tracked on the base Widget:

```cpp
struct WidgetState {
  bool hovered = false;
  bool pressed = false;
  bool focused = false;
  bool disabled = false;
};
```

Widget-specific data is stored in a tagged union on the Widget struct:

```cpp
struct PanelData {
  std::string title;
  bool show_title_bar = false;
  bool show_background = true;
  bool show_border = false;
};

struct TextData {
  RichText content;
  Align text_align = Align::kStart;
};

struct ButtonData {
  std::string label;
  std::string icon;            // atlas sprite name, empty = no icon
  std::function<void()> on_click;
};

struct TextInputData {
  std::string value;
  std::string placeholder;
  uint32_t cursor_pos = 0;     // byte offset in value
  uint32_t selection_start = 0;
  uint32_t selection_end = 0;
  bool show_cursor = true;
  float cursor_blink_timer = 0.0f;
  std::string ime_composition;  // current IME preedit string
  uint32_t ime_cursor = 0;
  std::function<void(const std::string&)> on_change;
  std::function<void(const std::string&)> on_submit;
};

struct ScrollContainerData {
  ScrollState scroll;
  bool show_h_scrollbar = false;
  bool show_v_scrollbar = true;
  float scrollbar_opacity = 1.0f;  // for auto-fade
  float idle_timer = 0.0f;
};

struct WidgetData {
  WidgetType type = WidgetType::kPanel;
  union {
    PanelData panel;
    TextData text;
    ButtonData button;
    TextInputData text_input;
    ScrollContainerData scroll_container;
  };
  // Constructor/destructor manage union lifetime
};
```

---

## 3. Panel

### 3.1 Behaviour

- A container for child widgets. Provides visual framing: background fill, border, rounded corners.
- Optional title bar rendered at the top of the panel with the title string.
- Background colour, border colour, and corner radius come from theme tokens.
- Shadows are M9 scope (blur pass); M1 uses a simple offset quad as a drop shadow placeholder.

### 3.2 Rendering

1. Emit a rounded-rect quad for the background (using `emitQuad` with corner_radius from theme).
2. If `show_border`: emit a border quad with `border_width` from theme.
3. If `show_title_bar`: emit a title bar rect at the top, render title text with the heading font.
4. Children are rendered inside the panel's content area (panel rect minus title bar height minus padding).

---

## 4. Text

### 4.1 Behaviour

- Displays static or dynamic text content using the text pipeline.
- Supports rich text spans (bold, italic, colour, code) via the RichText model.
- Text wrapping: multi-line wrapping when the widget's computed width is set; single-line with ellipsis truncation if text overflows a constrained width.
- Text alignment: start (left), center, or end (right) applied per line.

### 4.2 Rendering

1. Shape the RichText content via `shapeRichText()`.
2. Break lines via `breakLines()` using the widget's computed width.
3. Emit SDF glyph quads for each line, applying per-span colour.

---

## 5. Button

### 5.1 Behaviour

- Displays a label (text) and/or icon.
- Visual states driven by WidgetState: normal, hovered (highlight), pressed (darker), disabled (muted).
- On click: invokes `on_click` callback.
- Focusable: receives keyboard focus, activated by Enter key or gamepad Confirm.

### 5.2 State Colours

| State | Background Token | Text Token |
|-------|-----------------|------------|
| Normal | `Button.color.background` | `color.text` |
| Hovered | `Button.color.background.hover` | `color.text` |
| Pressed | `Button.color.background.active` | `color.text` |
| Disabled | `Button.color.background.disabled` | `color.muted` |

If hover/active/disabled tokens are not defined, the renderer derives them from the normal background (lighten for hover, darken for active, desaturate for disabled).

### 5.3 Rendering

1. Emit a rounded-rect background quad with the state-appropriate colour.
2. If icon is set: emit a textured quad for the icon on the left side.
3. Emit text label glyphs centered (or left-aligned if icon is present).

---

## 6. TextInput

### 6.1 Behaviour

- Single-line editable text field.
- Placeholder text shown when value is empty and not focused (rendered in muted colour).
- Cursor: a blinking vertical bar at `cursor_pos`. Blink rate: 0.5s on, 0.5s off.
- Selection: highlighted range between `selection_start` and `selection_end`. Shift+arrow keys extend selection. Click-and-drag selects.
- Keyboard shortcuts: Ctrl+A (select all), Ctrl+C (copy), Ctrl+V (paste), Ctrl+X (cut), Ctrl+Z (undo within field).
- IME: composition string rendered inline with an underline indicator.
- `on_change` fires on every character insertion/deletion. `on_submit` fires on Enter.
- Horizontal scrolling: if text is wider than the field, the view scrolls to keep the cursor visible.

### 6.2 Rendering

1. Emit a background rect with border (focused state uses accent border colour).
2. If value is empty and not focused: shape and render placeholder text in muted colour.
3. Otherwise: shape the value string, render glyphs.
4. Render selection highlight as a coloured rect behind selected glyphs.
5. Render cursor as a 2px-wide rect at the cursor glyph position (subject to blink timer).
6. If IME composition active: render composition text with an underline.

---

## 7. ScrollContainer

### 7.1 Behaviour

- Wraps child content that may exceed the container's bounds.
- Scroll direction controlled by `scroll_x` / `scroll_y` on the LayoutStyle.
- Mouse wheel scrolls vertically (Shift+wheel scrolls horizontally if `scroll_x` is enabled).
- Inertial scrolling: velocity decays per frame after mouse/touch release.
- Scroll offset clamped to `[0, content_size - visible_size]`.
- Scroll bars: rendered as overlay thin bars on the right (vertical) and/or bottom (horizontal). Auto-fade after 1s of no scroll activity.

### 7.2 Rendering

1. Push scissor rect to the container's `rect`.
2. Apply scroll offset as a translation to all child rendering.
3. Render children (clipped by scissor).
4. Pop scissor.
5. Render scroll bar overlays on top (not clipped) with current opacity.

### 7.3 Scroll Bar Sizing

- Bar length = `(visible_size / content_size) * track_length`. Minimum bar length: 24 px.
- Bar position = `(scroll_offset / max_offset) * (track_length - bar_length)`.

---

## 8. Widget Factory

A helper function creates widgets with sensible defaults:

```cpp
WidgetId createPanel(GuiWidgetTree&, WidgetId parent, const PanelData&);
WidgetId createText(GuiWidgetTree&, WidgetId parent, const RichText&);
WidgetId createButton(GuiWidgetTree&, WidgetId parent, const ButtonData&);
WidgetId createTextInput(GuiWidgetTree&, WidgetId parent, const TextInputData&);
WidgetId createScrollContainer(GuiWidgetTree&, WidgetId parent);
```

These call `createWidget()` from gui-widget-tree.h, then initialise the widget-specific data.

---

## 9. Error Strategy

| Situation | Handling |
|-----------|----------|
| Button on_click is null | Click is a no-op; no error |
| TextInput value exceeds render width | Horizontal scroll to keep cursor visible |
| ScrollContainer with no overflow | Scroll bars hidden; scroll offset stays 0 |
| Empty RichText on Text widget | Widget renders with zero height |

---

## 10. Edge Cases

- Button with empty label and empty icon: renders as a blank clickable rect (useful for custom-rendered buttons).
- TextInput receiving paste of very long string: accepted; horizontal scroll adjusts. No length limit in M1 (limit is a per-widget option for later).
- ScrollContainer with a single child smaller than the container: no scroll, content positioned at top-left.
- Disabled TextInput: cursor hidden, no keyboard events processed, background uses disabled theme tokens.

---

## 11. Module Decomposition

| File | Responsibility | Est. Lines |
|------|---------------|------------|
| `engine/gui/widgets.h` (included in gui-widget-tree.h) | PanelData, TextData, ButtonData, TextInputData, ScrollContainerData, WidgetData | ~100 |
| `engine/gui/widgets.cpp` | Widget factory functions, update logic (cursor blink, scroll bar fade) | ~300 |

---

## 12. Review Log

### Iteration 1
**Checklist results:** 10/11 pass, 1 fail
**Gaps identified:**
- No specification of clipboard interaction for TextInput copy/paste (R4 mentions Ctrl+C/V/X shortcuts but the platform clipboard API integration was not defined)

### Iteration 2
**Checklist results:** 11/11 pass
**Changes made:**
- Added note in section 6.1 that clipboard operations use SDL3 clipboard functions (`SDL_SetClipboardText` / `SDL_GetClipboardText`) and are routed through the platform abstraction layer

### Final
**All checklist items pass.** Approach finalised.
