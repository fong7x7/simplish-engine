# GUI Framework Documentation

The engine's retained-mode UI toolkit: widget tree, flexbox layout, FreeType text pipeline, RHI quad-batch renderer, theming, docking, and a markdown renderer. The code is in [`src/engine/gui/`](../../../src/engine/gui) and builds today.

> **Provenance.** These documents came from a 3D voxel engine along with the code. The framework itself is domain-neutral and applies unchanged; where a document describes an editor workspace or a domain widget, it reflects that project's editor, not the one specified in [Editor REQUIREMENTS](../../editor/REQUIREMENTS.md). Each such section is flagged in place.

## Start here

| Document | What it covers |
|---|---|
| [gui.md](gui.md) | The framework: goals, architecture, widget library, theming, input, non-functional requirements. **§9 is an editor contract inherited from another project** — treat it as a template, not a spec |
| [dockspace.md](dockspace.md) | Docking layout: regions, splitters, tabs, floating panels, workspace serialisation |

## Technical approaches

Implementation-level documents, one per subsystem. These describe how the shipped code works.

| Document | Subsystem | Implementation |
|---|---|---|
| [widgets.md](technical/widgets.md) | Widget base type, lifecycle, and the built-in widget set — §0 is the catalogue with examples: checkbox, toggle, radio, tabs, progress, number field, virtual list, table, tree view, card | `gui-widget.cpp`, `gui-button.cpp`, `gui-label.cpp`, `gui-panel.cpp`, `gui-dropdown.cpp`, `gui-slider.cpp`, `gui-scroll-panel.cpp`, `gui-text-input.cpp`, `gui-text-area.cpp`, `gui-image.cpp`, `gui-viewport.cpp` |
| [gui-widget-tree.md](technical/gui-widget-tree.md) | Tree ownership, IDs, traversal, dirty propagation | `gui-widget-tree.cpp` |
| [layout-engine.md](technical/layout-engine.md) | CSS flexbox layout: padding, margins, gaps, grow/shrink, wrap, alignment, absolute insets, natural sizes — with recipes | `flex-layout.cpp`, `layout-engine.cpp` |
| [renderer.md](technical/renderer.md) | Quad batching, scissor stack, draw-command emission | `gui-renderer.cpp`, `gui-draw-context.cpp` |
| [text-pipeline.md](technical/text-pipeline.md) | FreeType glyph rasterisation, font atlas, shaping, line breaking | `text-pipeline.cpp`, `shaped-run.cpp`, `gui-font-discovery.cpp` |
| [rhi-text-draw-path.md](technical/rhi-text-draw-path.md) | How text reaches the RHI | `gui-renderer.cpp` |
| [theming.md](technical/theming.md) | The typed theme: palette, spacing/radius/type/elevation scales, per-state component styles and their transitions, button variants, theme files — with recipes | `gui-theme.cpp`, `gui-theme-json.cpp`, `gui-state-style.cpp`, `gui-style-transition.cpp` |
| [motion.md](technical/motion.md) | Easing curves (cubic-bézier, spring), arriving and leaving (`GuiPresence`), layout glides, reduced motion — with recipes | `gui-easing.cpp`, `gui-motion-curves.cpp`, `gui-widget-presence.cpp`, `gui-widget-animator.cpp` |
| [inspecting.md](technical/inspecting.md) | Seeing what layout did: the layout overlay, `get_widgets`, the widget gallery captures | `gui-layout-overlay.cpp`, `editor-widgets-json.cpp`, `test_gui_gallery_capture.cpp` |
| [input.md](technical/input.md) | Hit testing, focus, event dispatch, input contexts | `gui-input.cpp` |
| [overlays.md](technical/overlays.md) | The overlay layer: popover placement, tooltips, modal dialogs, toasts, context menus, the menu bar | `gui-widget-tree-overlay.cpp`, `gui-popover-placement.cpp`, `gui-modal.cpp`, `gui-toasts.cpp`, `gui-menu-bar.cpp` |
| [dockspace.md](technical/dockspace.md) | Dock region arrangement and config loading | `dockspace-arrange.cpp`, `dockspace-config-loader.cpp`, `gui-dockspace-widget.cpp` |
| [markdown-parser.md](technical/markdown-parser.md) | Block and inline markdown parsing | `markdown-parser.cpp` |
| [markdown-renderer.md](technical/markdown-renderer.md) | Rendering parsed markdown into draw commands | `markdown-renderer.cpp` |
| [markdown-table-parser.md](technical/markdown-table-parser.md) | Table syntax parsing and column alignment | `markdown-parser.cpp` |
| [markdown-table-renderer.md](technical/markdown-table-renderer.md) | Table layout and rendering | `markdown-renderer.cpp` |
| [testing.md](technical/testing.md) | How GUI code is tested, including the software rasterizer | `gui-software-rasterizer.cpp` |

## What is implemented, and what is not

**Built and linked:** widget tree, layout engine, a typed theme with per-state styles and transitions, the FreeType text pipeline, the quad-batch RHI renderer, the software rasterizer used for testing, docking, widget animation and easing, image loading via stb_image, and the markdown parser and renderer including tables.

**Described but not present:** HarfBuzz shaping and ICU line breaking — FreeType is built with `FT_DISABLE_HARFBUZZ` and the in-tree `shaped-run` path handles kerning and ligatures without complex-script support ([gui.md §3](gui.md#3-dependencies)). The dev console described in the source project's docs was not carried over. Gamepad navigation is built — focus moves spatially between buttons, sliders, dropdowns and text fields, and a pad drives it through `GuiGamepadNavigator` ([technical/input.md §5](technical/input.md#5-gamepad-navigation)).

**Not yet exercised:** the GUI package builds and links, but nothing in this repository draws a frame with it yet — that arrives with the first client executable. Treat the widget behaviour described here as inherited-and-tested-elsewhere, not as verified in this tree.
