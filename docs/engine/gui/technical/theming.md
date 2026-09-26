# Simplish — Theming: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md §4.5](../gui.md#45-theming--customization)
**Library:** engine (`src/engine/gui/`)
**Date:** 2026-09-25 (rewritten when the typed theme replaced `GuiStyle` and the token map)

A widget never picks a colour. It asks the **theme** on its draw context,
and the theme answers with design tokens (a palette, scales for spacing,
radius, type and elevation) and, for each built-in widget, **what it looks
like in each interaction state**. Changing the theme restyles every widget
drawn from it, with no widget code changing. State changes (hover, press,
select) **blend** over the theme's transition time instead of snapping.

| File | What is in it |
|---|---|
| `gui-palette.h` | `GuiPalette`: colours by role, and the `GUI_PALETTE_DARK` / `GUI_PALETTE_LIGHT` presets |
| `gui-space.h`, `gui-radius.h`, `gui-text-size.h`, `gui-elevation.h` | The scale steps (`GuiSpace::MD`, `GuiRadius::LG`, …) and their defaults |
| `gui-shadow.h` | `GuiShadow`, a CSS `box-shadow` |
| `gui-widget-state.h` | `GuiWidgetState`: NORMAL, HOVER, PRESSED, FOCUSED, DISABLED, SELECTED |
| `gui-state-style.h`, `gui-state-styles.h` | One state's look (fill, text, border, radius, elevation); a look per state |
| `gui-button-variant.h` | `GuiButtonVariant`: NEUTRAL, PRIMARY, DANGER, GHOST |
| `gui-theme.h` | `GuiTheme`: all of the above, plus component styles derived from the palette |
| `gui-theme-json.h` | `parseGuiTheme` / `loadGuiTheme`: a theme from a file |
| `gui-style-transition.h` | `GuiStyleTransition`: eases a widget's drawn style between states |
| `gui-theme-constants.h` | `THEME_*`: the dark palette as constants, for code with no draw context |
| `test/test_gui_theme*.cpp`, `test_gui_style_transition.cpp`, `test_gui_widget_state.cpp` | The executable spec |

---

## 1. Where the theme comes from

```
GuiContext::theme  ──►  GuiDrawContext::theme  ──►  widget.render(ctx)
 (dark by default;       (RenderedGameClient::       ctx.activeTheme()
  applyTheme/loadTheme)   guiDrawContext sets it)
```

- `GuiContext` owns one `GuiTheme`, which is `GuiTheme::dark()` until you
  call `applyTheme(theme)` or `loadTheme(path, error)`.
- Every `GuiDrawContext` carries a pointer to it. `ctx.activeTheme()`
  returns that theme, or the dark preset when the pointer is null (tests,
  headless captures), so a widget can always read it.
- A subtree that should look different (a game screen inside the editor, a
  light dialog in a dark app) sets `state_styles` on its widgets, or is
  drawn with a context whose `theme` points elsewhere.

## 2. Tokens

```cpp
const GuiTheme& t = ctx.activeTheme();
t.palette.surface_raised          // colours by role, not by hue
t.space(GuiSpace::MD)             // 12
t.radius(GuiRadius::LG)           // 8
t.textSize(GuiTextSize::XL)       // 22 (drawn at size in the text phase)
t.shadow(GuiElevation::MID)       // {0, 4, 12, 0, palette.shadow}
t.transition_seconds              // 0.12
```

| Palette role | Used for |
|---|---|
| `background`, `surface`, `surface_raised`, `surface_sunken` | Window, panels, cards and menus, fields and tracks |
| `control`, `control_hover`, `control_pressed` | A neutral button's fill |
| `border`, `border_strong` | Hairlines; a hovered field |
| `text`, `text_muted`, `text_disabled` | Body, secondary, and disabled text |
| `primary` (`_hover`, `_pressed`), `on_primary` | The accent: primary action, selection, focused field; text on it |
| `danger` (`_hover`), `success`, `warning` | Status |
| `focus_ring`, `selection`, `scrim`, `shadow` | Keyboard focus, selected text, modal backdrop, drop shadows |

Use the steps in layout too:

```cpp
card->tree_layout.padding = {t.space(GuiSpace::LG), t.space(GuiSpace::LG),
                             t.space(GuiSpace::LG), t.space(GuiSpace::LG)};
card->tree_layout.gap = t.space(GuiSpace::SM);
```

## 3. States and component styles

A widget's **visual state** comes from its flags, in this order of
precedence:

| Flag | State | Set by |
|---|---|---|
| `disabled` | DISABLED | You. A disabled widget ignores clicks, `CONFIRM`, and is skipped by focus |
| `pressed` | PRESSED | The tree, while the button is held |
| `selected` | SELECTED | You: the active tool, the open menu's title, the current tab |
| `hovered` | HOVER | The tree |
| `focused` | FOCUSED | The tree, through `handleFocusChange`. A text field is also FOCUSED while typing |

`GuiWidget::visualState()` is virtual, so a widget with its own notion of
state can refine it (`GuiTextInput` does).

For each state the theme holds a `GuiStateStyle`: `fill`, `text`,
`border`, `border_width`, `radius`, `elevation`. The derived component
styles are:

| Theme member | Drawn by | Look |
|---|---|---|
| `button(NEUTRAL)` | `GuiButton` (default) | `control` fill; accent when SELECTED |
| `button(PRIMARY)` | `variant = PRIMARY` | Accent fill, `on_primary` text |
| `button(DANGER)` | `variant = DANGER` | `danger` fill |
| `button(GHOST)` | `variant = GHOST` | No box until hovered; `control` when SELECTED |
| `field` | `GuiTextInput`, `GuiTextArea` | Sunken, hairline border; `border_strong` on hover; accent border while typing |
| `card` | A `GuiPanel` given `state_styles = theme.card` | Raised (`LOW`), lifting to `MID` on hover |
| `menu` | `GuiDropdown` (unless its `style` overrides colours) | Raised surface, border, `MID` |

`GuiSlider`, `GuiScrollPanel` (thumb) and the focus ring read the palette
directly. `GuiLabel::color` is optional; unset means `palette.text`.

## 4. Recipes

### 4.1 Buttons by role

```cpp
save->variant = GuiButtonVariant::PRIMARY;
remove->variant = GuiButtonVariant::DANGER;
close->variant = GuiButtonVariant::GHOST;
save->disabled = !document_dirty;          // dimmed, unclickable, unfocusable
```

### 4.2 A toggle group (toolbar tools, tabs)

```cpp
for (size_t i = 0; i < tools.size(); ++i) {
  button(i)->selected = i == active;       // SELECTED wins over HOVER
}
```

This is `EditorToolbarWidget::styleButtons`. The editor's menu bar marks
the open menu's title the same way.

### 4.3 A card, or any panel with states

```cpp
panel->state_styles = ctx_theme.card;      // raised, lifts on hover
tile->state_styles = ctx_theme.card;
tile->selected = is_current;               // accent border
```

A `GuiPanel` with no `state_styles` draws its `fill_color`,
`border_color`, `border_width` and `corner_radius` fields as before.

### 4.4 A one-off look

```cpp
GuiStateStyles look = GuiStateStyles::uniform(
    {.fill = gold, .text = ink, .radius = 12.0f});
look.of(GuiWidgetState::HOVER).fill = gold_light;
look.of(GuiWidgetState::DISABLED).fill = GuiColor::applyOpacity(gold, 0.5f);
reward->state_styles = look;               // wins over the theme
```

`game/ui`'s `uiButtonLook` builds a game screen's buttons this way, from
the screen file's colours.

### 4.5 A custom widget that follows the theme

```cpp
const GuiStateStyles* MyChip::themeStyles(const GuiTheme& t) const {
  return &t.button(GuiButtonVariant::GHOST);
}
void MyChip::render(const GuiDrawContext& ctx) const {
  const GuiStateStyle s = drawnStyle(ctx);   // mid-blend while changing
  ctx.drawBox(rect, s, opacity);             // fill + border at radius
  ctx.drawCenteredText(rect, GuiColor::applyOpacity(s.text, opacity), text);
}
```

`drawnStyle` returns the running blend while `update` is transitioning,
and otherwise the look for the current state. That means a widget renders
correctly even in a test that never calls `update`. The blend only runs if
the widget's `update(ctx, dt)` is called each frame, which
`GuiWidgetTree::updateAll` does; a subclass overriding `update` must call
`GuiWidget::update`.

### 4.6 A theme file

```json
{ "name": "Ember", "base": "dark",
  "palette": { "primary": "#e8703a", "primary_hover": "#f08a52",
               "surface": "#221c1a" },
  "radii": [0, 4, 6, 10],
  "transition_ms": 150 }
```

```cpp
std::string error;
if (!gui.loadTheme("content/ui/theme.json", error)) {
  Logger::warn("gui", error);              // e.g. "palette.primray: not a palette colour"
}
```

Every key is optional. `base` (`dark` or `light`) is the preset the rest
is laid over. `spacing`, `radii` and `text_sizes` replace their scales
from the first step; missing steps keep their defaults. Component styles
and shadows are then derived from the result. Colours are `#rrggbb` or
`#rrggbbaa`. `parseGuiColor` reads them, and `game/ui` uses the same
function. After editing a `GuiTheme` in code, call `deriveComponents()`.

## 5. Rules

- **New drawing code reads `ctx.activeTheme()`.** `THEME_*` constants
  exist for the editor's older hand-drawn widgets and for pixel-matching
  tests. They are the dark palette and will not follow a theme change.
- **Prefer a role to a colour.** A new palette role is better than a
  literal that only looks right in one theme.
- **Interaction goes through flags** (`disabled`, `selected`), never by
  swapping styles by hand each tick. The transition only animates a change
  of state.
- **Elevation draws a shadow.** `drawBox` draws the theme's `shadow(elevation)`
  under any filled style that is raised; `GuiDropdown` does, from
  `theme.menu`.

## 6. What is not here

- Per-subtree theme scopes. The old string-keyed `ThemeScopeStack` was
  never used and is gone. Use `state_styles` or a second draw context.
- Named style classes (`"style": "inventory-slot"`). A game screen's
  per-node colours cover the current need.
- Typography roles are built: `theme.font(GuiTextRole::HEADING)`, derived
  from `text_sizes`; see [text-pipeline.md §0.2](text-pipeline.md#02-roles).
