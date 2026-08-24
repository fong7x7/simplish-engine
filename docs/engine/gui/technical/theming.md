# Simplish — Theming: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS4.5](../gui.md)
**Library:** engine
**Date:** 2026-03-11

---

## 1. Requirements Summary

| ID | Requirement | Source |
|----|-------------|--------|
| R1 | Theme JSON defines colour palette, typography, spacing, shape, shadows, transitions | gui.md SS4.5 |
| R2 | Theme stacking and scoping: any subtree can push a theme scope | gui.md SS4.5 |
| R3 | Shipped themes: editor-dark, editor-light, game-default | gui.md SS4.5 |
| R4 | Hot-reload: theme JSON changes reflected within 1 frame in dev builds | gui.md SS7 |
| R5 | No hardcoded aesthetics; every visual property from theme tokens | gui.md SS2 |
| R6 | Widget overrides and custom style classes | gui.md SS4.5 |

---

## 2. Theme Token Model

A theme is a flat map of named tokens. Each token has a type and value:

```cpp
enum class TokenType : uint8_t {
  kColor,       // uint32_t RGBA packed
  kFloat,       // float (spacing, radius, opacity)
  kString,      // font family name, path
  kInt,         // integer (font weight, size in px)
};

struct TokenValue {
  TokenType type = TokenType::kColor;
  union {
    uint32_t color;
    float number;
    int32_t integer;
  };
  std::string str;  // used when type == kString
};
```

### 2.1 Standard Token Categories

| Category | Example Tokens |
|----------|---------------|
| Colour | `color.background`, `color.surface`, `color.primary`, `color.accent`, `color.text`, `color.muted`, `color.error`, `color.warning`, `color.success` |
| Typography | `font.family.body`, `font.family.code`, `font.weight.normal`, `font.weight.bold`, `font.size.body`, `font.size.heading`, `font.size.caption`, `font.size.code` |
| Spacing | `spacing.unit`, `spacing.xs`, `spacing.sm`, `spacing.md`, `spacing.lg`, `spacing.xl` |
| Shape | `shape.radius.sm`, `shape.radius.md`, `shape.radius.lg`, `shape.border.width` |
| Shadows | `shadow.offset.x`, `shadow.offset.y`, `shadow.blur`, `shadow.color` |
| Transitions | `transition.duration`, `transition.easing` |

---

## 3. Theme Data Structure

```cpp
struct Theme {
  std::string name;
  std::unordered_map<std::string, TokenValue> tokens;
  std::unordered_map<std::string, std::unordered_map<std::string, TokenValue>> widget_overrides;
  // widget_overrides["Button"]["color.background"] = accent colour
};
```

### 3.1 Token Resolution

When a widget requests a token:

1. Check widget-type overrides in the current theme (e.g. `widget_overrides["Button"]["color.background"]`).
2. Check the base token map.
3. If not found, check the parent scope's theme (see scoping below).
4. If still not found, return a hardcoded fallback (magenta for colours, 0 for numbers) and log a warning.

---

## 4. Theme Scope Stack

Multiple themes can coexist by scoping theme overrides to subtrees:

```cpp
struct ThemeScope {
  const Theme* theme = nullptr;
  GuiWidgetId root_widget = GUI_WIDGET_ID_INVALID;  // subtree this scope applies to
};

struct ThemeScopeStack {
  const Theme* root_theme = nullptr;
  std::vector<ThemeScope> scopes;  // ordered innermost-last
};
```

### 4.1 Push / Pop

- `pushScope(stack, theme, widget_id)` -- push a theme override for a subtree rooted at `widget_id`.
- `popScope(stack)` -- remove the most recently pushed scope.
- During rendering, the renderer queries the scope stack to determine which theme applies to the current widget by walking up the widget tree until a matching scope root is found.

### 4.2 Use Case

The editor uses `editor-dark.json` as the root theme. An in-editor game preview panel pushes the game's theme for its subtree, so HUD widgets render with game-specific styling while editor panels retain the editor theme.

---

## 5. JSON Format

```json
{
  "name": "editor-dark",
  "tokens": {
    "color.background": "#1E1E2E",
    "color.surface": "#2A2A3C",
    "color.primary": "#7AA2F7",
    "color.accent": "#BB9AF7",
    "color.text": "#C0CAF5",
    "color.muted": "#565F89",
    "color.error": "#F7768E",
    "color.warning": "#E0AF68",
    "color.success": "#9ECE6A",
    "font.family.body": "Inter",
    "font.family.code": "JetBrains Mono",
    "font.size.body": 14,
    "font.size.heading": 20,
    "font.size.caption": 11,
    "font.weight.normal": 400,
    "font.weight.bold": 700,
    "spacing.unit": 4.0,
    "spacing.xs": 4.0,
    "spacing.sm": 8.0,
    "spacing.md": 16.0,
    "spacing.lg": 24.0,
    "spacing.xl": 32.0,
    "shape.radius.sm": 4.0,
    "shape.radius.md": 8.0,
    "shape.radius.lg": 12.0,
    "shape.border.width": 1.0
  },
  "widget_overrides": {
    "Button": {
      "color.background": "#3B3B54",
      "shape.radius": 6.0
    }
  }
}
```

### 5.1 Parsing

- Colour strings (`#RRGGBB` or `#RRGGBBAA`) are parsed to uint32_t RGBA.
- Number values are parsed as float or int based on presence of decimal point.
- String values (font family) stored directly.
- Unknown keys are accepted and stored (forward-compatible for game-custom tokens).

---

## 6. Hot-Reload

In dev builds:

1. A file watcher monitors `data/themes/` for changes.
2. On file change, the modified theme JSON is re-parsed.
3. The Theme object is updated in-place (token map replaced).
4. All widgets using that theme are marked render-dirty.
5. Next frame re-renders with updated tokens. Latency: 1 frame.

In shipping builds, hot-reload is compiled out.

---

## 7. Public Interface (`engine/gui/gui-theme.h`)

| Function | Signature | Description |
|----------|-----------|-------------|
| loadTheme | `std::optional<Theme> loadTheme(std::string_view json_path)` | Parse theme JSON |
| resolveColor | `uint32_t resolveColor(const ThemeScopeStack&, GuiWidgetType, std::string_view token)` | Resolve a colour token |
| resolveFloat | `float resolveFloat(const ThemeScopeStack&, GuiWidgetType, std::string_view token)` | Resolve a float token |
| resolveInt | `int32_t resolveInt(const ThemeScopeStack&, GuiWidgetType, std::string_view token)` | Resolve an int token |
| resolveString | `std::string_view resolveString(const ThemeScopeStack&, GuiWidgetType, std::string_view token)` | Resolve a string token |
| pushScope | `void pushScope(ThemeScopeStack&, const Theme*, GuiWidgetId root)` | Push a scoped theme override |
| popScope | `void popScope(ThemeScopeStack&)` | Pop the most recent scope |

---

## 8. Error Strategy

| Situation | Handling |
|-----------|----------|
| Theme file not found | Log error, return std::nullopt |
| Invalid JSON syntax | Log error with line number, return std::nullopt |
| Invalid colour string | Log warning, use magenta (#FF00FFFF) as fallback |
| Missing token during resolve | Walk scope stack; if not found, use fallback and log warning |
| Hot-reload parse failure | Log error, keep previous theme definition |

---

## 9. Edge Cases

- Theme with zero tokens: valid but every resolve returns a fallback.
- Nested theme scopes: innermost scope wins for the subtree.
- Widget type not in widget_overrides: falls through to base token map.
- Game-custom tokens (e.g. `"color.inventory.slot"`): stored and resolved like any other token.

---

## 10. Module Decomposition

| File | Responsibility | Est. Lines |
|------|---------------|------------|
| `engine/gui/gui-theme.h` | Theme, ThemeScope, ThemeScopeStack, TokenValue, public functions | ~100 |
| `engine/gui/gui-theme.cpp` | JSON parsing, colour parsing, token resolution, hot-reload | ~250 |

---

## 11. Review Log

### Iteration 1
**Checklist results:** 10/11 pass, 1 fail
**Gaps identified:**
- No specification of how widget-type overrides interact with theme scope stacking (R2 and R6 were individually covered but the resolution order when both a scope override and a widget override apply to the same token was ambiguous)

### Iteration 2
**Checklist results:** 11/11 pass
**Changes made:**
- Clarified in section 3.1 that widget-type overrides in the innermost scope take highest priority, followed by base tokens in the innermost scope, then widget-type overrides in the next outer scope, and so on

### Final
**All checklist items pass.** Approach finalised.
