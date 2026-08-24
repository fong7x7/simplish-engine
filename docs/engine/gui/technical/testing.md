# GUI Testing — Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Version:** 0.1
**Last Updated:** 2026-03-27

---

## Overview

**Goal:** Provide a Playwright-inspired automated testing framework for all GUI widget systems. Every widget that builds a subtree in `GuiWidgetTree` — engine widgets, editor panels, game HUD — must have automated tests that verify structure, interactions, layout, visibility, style, and lifecycle.

**Requirement:** All GUI systems that use `GuiWidgetTree` must have automated tests before merging. See `docs/development/test-planning.md §8` for the full testing strategy and `CLAUDE.md` Quality Gates for enforcement.

---

## Test Harness Architecture

```
Test Code
    |
    ├── GuiLocator        Find widgets (byId, byType, byText, childOf, nth)
    ├── GuiTestHarness    Owns GuiWidgetTree + mock renderer + draw context
    ├── GuiExpect          Structural + visual assertions (Catch2 integration)
    └── GuiRenderExpect   Render-layer assertions (quad capture)
```

### Components

| Component | Location | Purpose |
|-----------|----------|---------|
| `GuiTestHarness` | `src/engine/gui/test/support/gui_test_harness.h` | Test environment: creates tree, mock renderer, 1920x1080 viewport. Drives `layout()`, `tick()`, `tickUntilStable()`, `hasAnimatingWidget()`. Convenience builders: `addPanel()`, `addButton()`, `addTextInput()`, `addLabel()`. |
| `GuiLocator` | `src/engine/gui/test/support/gui_locator.h` | Widget finder: `byId`, `byWidgetId`, `byType`, `byText`, `nth`, `childOf`. |
| `GuiExpect` | `src/engine/gui/test/support/gui_expect.h` | Assertions: existence, visibility, focus, hover, text, opacity, child count, rect, fill color, border color, corner radius, border width, button style, relative position (`toBeAbove/Below/LeftOf/RightOf`), overlap, containment, z-order. Use `expect(loc)` for REQUIRE or `expectSoft(loc)` for CHECK. |
| `GuiRenderExpect` | `src/engine/gui/test/support/gui_render_expect.h` | Render assertions: `capture()` / `captureSoft()` to render the tree, then assert on quads. `clearRender()` resets between captures. |
| `MockPlatformUtility` | `tests/engine/core/support/mock_platform_utility.h` | Immediate-result mock for platform operations (file dialogs). |

### Action API

```
click(locator)           — dispatch mouseDown + mouseUp at widget center
doubleClick(locator)     — two rapid clicks
rightClick(locator)      — dispatch right-click at widget center
hover(locator)           — dispatch mouseMove to widget center
typeText(locator, text)  — focus widget, dispatch key events for each char
clearAndType(loc, text)  — select all, delete, then type
scroll(locator, dx, dy)  — dispatch scroll event
focus(locator)           — set keyboard focus
pressKey(harness, code, mods)  — dispatch a key event (mods optional)
```

### Resolution API

```
resolve(locator)         — return the first matched GuiWidget* (or nullptr)
resolveAll(locator)      — return all matched widget IDs
count(locator)           — return number of matches
extractText(widget)      — extract visible text from any widget type
```

---

## Coverage Requirements

Every widget must have tests covering all six categories:

| # | Category | Minimum assertions |
|---|----------|-------------------|
| 1 | **Structure** | Children exist after `init()`, correct parent-child hierarchy |
| 2 | **Interactions** | Click handlers fire, text input works, focus/blur correct |
| 3 | **Layout** | Relative positioning between sibling widgets |
| 4 | **Visibility** | Show/hide states, modal overlays, collapse/expand |
| 5 | **Style** | Colors, borders match theme expectations |
| 6 | **Lifecycle** | `shutdown()` removes all widgets from tree |

---

## Assertion Categories

### Structural Assertions

- `toExist()` / `toNotExist()` — widget found / not found
- `toHaveChildCount(n)` — direct children count
- `toHaveText("exact")` / `toContainText("sub")` — text content

### Collection Assertions

- `toHaveCount(n)` — expected number of locator matches

### State Assertions

- `toBeVisible()` / `toBeHidden()` — visibility flag
- `toBeFocused()` / `toBeHovered()` / `toBePressed()` — interaction state
- `toHaveValue(v, epsilon)` — slider/numeric value

### Visual Assertions

- `toHaveFillColor(color)` — panel background color
- `toHaveBorderColor(color)` — panel border color
- `toHaveCornerRadius(r)` — panel corner radius
- `toHaveBorderWidth(w)` — panel border width
- `toHaveButtonStyle(field, color)` — button style fields (`GuiButtonStyleField::BG_COLOR`, `TEXT_COLOR`, `HOVER_COLOR`)
- `toHaveRect(rect, tolerance)` — computed layout rect
- `toHaveOpacity(expected, epsilon)` — widget opacity

### Position Assertions

- `toBeAbove(other)` / `toBeBelow(other)` — vertical ordering
- `toBeLeftOf(other)` / `toBeRightOf(other)` — horizontal ordering
- `toOverlap(other)` — rect intersection
- `toBeInside(other)` — rect containment
- `toHaveZIndexAbove(other)` — draw order

### Render Assertions

- `toHaveRenderedQuads()` — at least one quad was rendered
- `toHaveNoRenderedQuads()` — nothing was rendered
- `toHaveQuadCount(min, max)` — quad count in range
- `toHaveQuadWithColor(color)` — any quad matches color
- `toHaveQuadInRect(rect)` — any quad in region
- `toHaveQuadWithColorInRect(color, rect)` — combined match
- `clearRender()` — reset accumulated vertices between captures
