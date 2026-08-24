# Simplish — GUI Framework: Technical Approach (Phase 1)

**Parent document:** [architecture.md](../../REQUIREMENTS.md)
**Status:** Review Cycle 2 Complete
**Date:** 2026-03-11

---

## Overview

**Goal:** Design the M1 GUI framework — a custom retained-mode UI system rendered through the engine RHI. M1 delivers the core widget tree, flexbox-style layout engine, FreeType + HarfBuzz text pipeline, quad-batch renderer, basic widgets (Panel, Text, Button, TextInput, ScrollContainer), JSON-driven dark theme, and dev console overlay.

Editor-only widgets (DockArea, NodeGraph, ChatPanel) and game runtime UI (HUD, dialog) are deferred to M7/M9.

## Subsystem Index

| Topic | Document | Summary |
|-------|----------|---------|
| Widget Tree | [gui/gui-widget-tree.md](gui-widget-tree.md) | Retained-mode tree: create, update, destroy, dirty flags, traversal |
| Layout Engine | [gui/layout-engine.md](layout-engine.md) | Flexbox-style layout: row/column flex, alignment, padding, margin, scroll |
| Text Pipeline | [gui/text-pipeline.md](text-pipeline.md) | FreeType rasterization, HarfBuzz shaping, SDF atlas, line breaking, rich text spans |
| Renderer | [gui/renderer.md](renderer.md) | Quad-batch RHI renderer: vertex batching, textured text, scissor clipping |
| RHI text draw path | [gui/rhi-text-draw-path.md](rhi-text-draw-path.md) | Atlas upload, `emitGlyph` UVs, desktop stub `RhiDevice`, no SDL in component draw |
| Theming | [gui/theming.md](theming.md) | JSON theme loading, token resolution, scoped overrides, hot-reload |
| Input | [gui/input.md](input.md) | Mouse hit testing, keyboard focus, basic gamepad d-pad navigation |
| Widgets | [gui/widgets.md](widgets.md) | M1 widgets: Panel, Text, Button, TextInput, ScrollContainer |
| Testing | [gui/testing.md](testing.md) | Playwright-inspired test harness, locators, visual assertions, render capture |
| Dockspace | [gui/dockspace.md](dockspace.md) | Generic region allocator: top/bottom/left/right/centre dock regions, config-driven, enforces no-overlap invariant. Replaces per-panel manual rect math. |

## Architecture

```
                    Game / Editor Code
                           |
                    +--------------+
                    |  GuiContext   |  (owns widget tree, theme, font cache)
                    +--------------+
                           |
        +--------+---------+---------+--------+
        |        |         |         |        |
   WidgetTree  Layout   TextPipe  Theme   InputRouter
        |        |         |         |        |
        +--------+---------+---------+--------+
                           |
                    +--------------+
                    | GuiRenderer  |  (quad batching -> RHI)
                    +--------------+
                           |
                      RhiDevice
```

## Key Design Decisions

1. **Retained-mode tree** -- Persistent widget tree diffed per frame; enables animation, partial re-layout, and accessibility traversal
2. **Flexbox layout model** -- Row/column containers with alignment, wrapping, and scroll; familiar model, covers M1-M9 needs
3. **Atlas text** -- FreeType rasterizes glyphs into a CPU/GPU atlas; `emitGlyph` emits textured quads (SDF atlas deferred)
4. **Quad batching** -- All UI quads merged into one vertex buffer per draw call where possible; scissor stack for clipping
5. **Theme-driven** -- Every visual property resolved from JSON theme tokens; no hardcoded aesthetics
6. **No exceptions** -- All fallible operations return `std::optional` or `bool`; log before return
